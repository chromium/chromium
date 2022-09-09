// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include <list>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_local.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/logging.h"

namespace {

base::LazyInstance<base::ThreadLocalPointer<Session>>::DestructorAtExit
    lazy_tls_session = LAZY_INSTANCE_INITIALIZER;

}  // namespace

FrameInfo::FrameInfo(const std::string& parent_frame_id,
                     const std::string& frame_id,
                     const std::string& chromedriver_frame_id)
    : parent_frame_id(parent_frame_id),
      frame_id(frame_id),
      chromedriver_frame_id(chromedriver_frame_id) {}

InputCancelListEntry::InputCancelListEntry(base::DictionaryValue* input_state,
                                           const MouseEvent* mouse_event,
                                           const TouchEvent* touch_event,
                                           const KeyEvent* key_event)
    : input_state(input_state) {
  if (mouse_event != nullptr) {
    this->mouse_event = std::make_unique<MouseEvent>(*mouse_event);
    this->mouse_event->type = kReleasedMouseEventType;
  } else if (touch_event != nullptr) {
    this->touch_event = std::make_unique<TouchEvent>(*touch_event);
    this->touch_event->type = kTouchEnd;
  } else if (key_event != nullptr) {
    this->key_event = std::make_unique<KeyEvent>(*key_event);
    this->key_event->type = kKeyUpEventType;
  }
}

InputCancelListEntry::InputCancelListEntry(InputCancelListEntry&& other) =
    default;

InputCancelListEntry::~InputCancelListEntry() = default;

BidiConnection::BidiConnection(int connection_id,
                               SendTextFunc send_response,
                               CloseFunc close_connection)
    : connection_id(connection_id),
      send_response(std::move(send_response)),
      close_connection(std::move(close_connection)) {}

BidiConnection::BidiConnection(BidiConnection&& other) = default;

BidiConnection::~BidiConnection() = default;

BidiConnection& BidiConnection::operator=(BidiConnection&& other) = default;

// The default timeout values came from W3C spec.
const base::TimeDelta Session::kDefaultImplicitWaitTimeout = base::Seconds(0);
const base::TimeDelta Session::kDefaultPageLoadTimeout = base::Seconds(300);
const base::TimeDelta Session::kDefaultScriptTimeout = base::Seconds(30);
const int kBidiQueueCapacity = 20;

Session::Session(const std::string& id)
    : id(id),
      w3c_compliant(kW3CDefault),
      quit(false),
      detach(false),
      sticky_modifiers(0),
      mouse_position(0, 0),
      pressed_mouse_button(kNoneMouseButton),
      implicit_wait(kDefaultImplicitWaitTimeout),
      page_load_timeout(kDefaultPageLoadTimeout),
      script_timeout(kDefaultScriptTimeout),
      strict_file_interactability(false),
      click_count(0),
      mouse_click_timestamp(base::TimeTicks::Now()) {}

Session::Session(const std::string& id, std::unique_ptr<Chrome> chrome)
    : Session(id) {
  this->chrome = std::move(chrome);
}

Session::Session(const std::string& id, const std::string& host) : Session(id) {
  this->host = host;
}

Session::~Session() {}

Status Session::GetTargetWindow(WebView** web_view) {
  if (!chrome)
    return Status(kNoSuchWindow, "no chrome started in this session");

  Status status = chrome->GetWebViewById(window, web_view);
  if (status.IsError())
    status = Status(kNoSuchWindow, "target window already closed", status);
  return status;
}

void Session::SwitchToTopFrame() {
  frames.clear();
  SwitchFrameInternal(true);
}

void Session::SwitchToParentFrame() {
  if (!frames.empty())
    frames.pop_back();
  SwitchFrameInternal(false);
}

void Session::SwitchToSubFrame(const std::string& frame_id,
                               const std::string& chromedriver_frame_id) {
  std::string parent_frame_id;
  if (!frames.empty())
    parent_frame_id = frames.back().frame_id;
  frames.push_back(FrameInfo(parent_frame_id, frame_id, chromedriver_frame_id));
  SwitchFrameInternal(false);
}

std::string Session::GetCurrentFrameId() const {
  if (frames.empty())
    return std::string();
  return frames.back().frame_id;
}

std::vector<WebDriverLog*> Session::GetAllLogs() const {
  std::vector<WebDriverLog*> logs;
  for (const auto& log : devtools_logs)
    logs.push_back(log.get());
  if (driver_log)
    logs.push_back(driver_log.get());
  return logs;
}

void Session::SwitchFrameInternal(bool for_top_frame) {
  WebView* web_view = nullptr;
  Status status = GetTargetWindow(&web_view);
  if (!status.IsError()) {
    if (for_top_frame)
      web_view->SetFrame(std::string());
    else
      web_view->SetFrame(GetCurrentFrameId());
  } else {
    // Do nothing; this should be very rare because callers of this function
    // have already called GetTargetWindow.
    // Let later code handle issues that arise from the invalid state.
  }
}

bool Session::BidiMapperIsLaunched() const {
  return bidi_mapper_is_launched_;
}

void Session::OnBidiResponse(const std::string& payload) {
  absl::optional<base::Value> payload_parsed =
      base::JSONReader::Read(payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!payload_parsed || !payload_parsed->is_dict()) {
    LOG(WARNING) << "BiDi response is not a map: " << payload;
    return;
  }

  if (payload_parsed->GetDict().FindBool("launched").value_or(false)) {
    bidi_mapper_is_launched_ = true;
    return;
  }

  // If there is no active bidi connections the events will be accumulated.
  bidi_response_queue_.push(std::move(*payload_parsed));
  for (; bidi_response_queue_.size() > kBidiQueueCapacity;
       bidi_response_queue_.pop()) {
    LOG(WARNING) << "BiDi response queue overflow, dropping the message: "
                 << bidi_response_queue_.front();
  }
  ProcessBidiResponseQueue();
}

void Session::AddBidiConnection(int connection_id,
                                SendTextFunc send_response,
                                CloseFunc close_connection) {
  bidi_connections_.emplace_back(connection_id, std::move(send_response),
                                 std::move(close_connection));
  ProcessBidiResponseQueue();
}

void Session::RemoveBidiConnection(int connection_id) {
  // Reallistically we will not have many connections, therefore linear search
  // is optimal.
  auto it = std::find_if(bidi_connections_.begin(), bidi_connections_.end(),
                         [connection_id](const auto& conn) {
                           return conn.connection_id == connection_id;
                         });
  if (it != bidi_connections_.end()) {
    bidi_connections_.erase(it);
  }
}

void Session::ProcessBidiResponseQueue() {
  if (bidi_connections_.empty() || bidi_response_queue_.empty()) {
    return;
  }
  // Only single websocket connection is supported now.
  DCHECK(bidi_connections_.size() == 1);
  for (; !bidi_response_queue_.empty(); bidi_response_queue_.pop()) {
    // TODO(chromedriver:4179): In the future we will support multiple
    // connections. The payload will have to be parsed and routed to the
    // appropriate connection. The events will have to be delivered to all
    // connections.
    base::Value response_parsed = std::move(bidi_response_queue_.front());
    std::string response;
    if (!base::JSONWriter::Write(response_parsed, &response)) {
      LOG(WARNING) << "unable to serialize a BiDi response";
      continue;
    }
    for (const BidiConnection& conn : bidi_connections_) {
      // If the callback fails (asynchronously) because the connection was
      // broken we simply ignore this fact as the message cannot be delivered
      // over that connection anyway.
      conn.send_response.Run(response);
      absl::optional<int> response_id = response_parsed.GetDict().FindInt("id");
      if (response_id && *response_id == awaited_bidi_response_id) {
        awaited_bidi_response_id = -1;
        // No "id" means that we are dealing with an event
      }
    }
  }
}

void Session::CloseAllConnections() {
  for (BidiConnection& conn : bidi_connections_) {
    // If the callback fails (asynchronously) because the connection was
    // terminated we simply ignore this - it is already closed.
    conn.close_connection.Run();
  }
}

Session* GetThreadLocalSession() {
  return lazy_tls_session.Pointer()->Get();
}

void SetThreadLocalSession(std::unique_ptr<Session> session) {
  lazy_tls_session.Pointer()->Set(session.release());
}
