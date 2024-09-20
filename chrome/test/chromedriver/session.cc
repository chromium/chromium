// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session.h"

#include <list>
#include <utility>

#include <string.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/logging.h"

namespace {

constinit thread_local Session* session = nullptr;

}  // namespace

namespace internal {

Status SplitChannel(std::string* channel,
                    int* connection_id,
                    std::string* suffix) {
  DCHECK(channel);  // precondition
  size_t k = channel->size();
  for (; k && (*channel)[k - 1] != '/'; --k) {
  }
  if (k == 0) {
    return Status{kUnknownError,
                  "channel does not end with an expected suffix"};
  }
  *suffix = channel->substr(k - 1);
  channel->erase(std::next(channel->begin(), k - 1), channel->end());
  --k;

  for (; k && (*channel)[k - 1] != '/'; --k) {
  }
  if (k == 0) {
    return Status{kUnknownError, "channel does not contain connection_id"};
  }
  std::string connection_str = channel->substr(k);
  channel->erase(std::next(channel->begin(), k - 1), channel->end());
  if (!base::StringToInt(connection_str, connection_id)) {
    return Status{kUnknownError,
                  "connection_id in the channel must be integer"};
  }

  return Status{kOk};
}
}  // namespace internal

FrameInfo::FrameInfo(const std::string& parent_frame_id,
                     const std::string& frame_id,
                     const std::string& chromedriver_frame_id)
    : parent_frame_id(parent_frame_id),
      frame_id(frame_id),
      chromedriver_frame_id(chromedriver_frame_id) {}

InputCancelListEntry::InputCancelListEntry(base::Value::Dict* input_state,
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
// The extra timeout values.
const base::TimeDelta Session::kDefaultBrowserStartupTimeout =
    base::Seconds(60);
const char Session::kChannelSuffix[] = "/chan";
const char Session::kNoChannelSuffix[] = "/nochan";
const char Session::kBlockingChannelSuffix[] = "/blocking";

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

Status Session::OnBidiResponse(base::Value::Dict payload) {
  std::string* channel = payload.FindString("channel");
  if (!channel) {
    return Status{kUnknownError, "channel is missing in the BiDi response"};
  }

  if (base::EndsWith(*channel, kBlockingChannelSuffix)) {
    if (!awaiting_bidi_response) {
      return Status{kUnknownError, "unexpected blocking BiDi response"};
    }
    awaiting_bidi_response = false;
    size_t pos = channel->size() - strlen(kBlockingChannelSuffix);
    // Update the channel value of the payload in-place.
    channel->erase(std::next(channel->begin(), pos), channel->end());
  }

  int connection_id = -1;
  std::string suffix;
  Status status = internal::SplitChannel(channel, &connection_id, &suffix);
  if (status.IsError()) {
    return status;
  }

  if (suffix == kNoChannelSuffix) {
    payload.Remove("channel");
  } else if (suffix != kChannelSuffix) {
    return Status{kUnknownError,
                  "unexpected channel name in the BiDi response"};
  }

  std::string message;
  // `OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION` is needed to keep the BiDi format.
  // crbug.com/chromedriver/4297.
  if (!base::JSONWriter::WriteWithOptions(
          payload, base::JSONWriter::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION,
          &message)) {
    return Status{kUnknownError, "unable to serialize a BiDi response"};
  }

  auto it = base::ranges::find(bidi_connections_, connection_id,
                               &BidiConnection::connection_id);
  if (it == bidi_connections_.end()) {
    // It can happen that we receive a message from the mapper designated to the
    // channel that has recently been closed.
    LOG(INFO) << "BiDi connection is closed. Skipping the BiDiMapper message: "
              << message;
    return Status{kOk};
  }

  it->send_response.Run(std::move(message));
  return Status{kOk};
}

void Session::AddBidiConnection(int connection_id,
                                SendTextFunc send_response,
                                CloseFunc close_connection) {
  bidi_connections_.emplace_back(connection_id, std::move(send_response),
                                 std::move(close_connection));
}

void Session::RemoveBidiConnection(int connection_id) {
  // As connections can be closed by both remote and local ends
  // we don't treat an attempt to close a non-existing (presumably closed)
  // connection as an error.
  // Reallistically we will not have many connections, therefore linear search
  // is optimal.
  auto it = base::ranges::find(bidi_connections_, connection_id,
                               &BidiConnection::connection_id);
  if (it != bidi_connections_.end()) {
    bidi_connections_.erase(it);
  }
}

void Session::CloseAllConnections() {
  for (BidiConnection& conn : bidi_connections_) {
    // If the callback fails (asynchronously) because the connection was
    // terminated we simply ignore this - it is already closed.
    conn.close_connection.Run();
  }
  bidi_connections_.clear();
}

Session* GetThreadLocalSession() {
  return session;
}

void SetThreadLocalSession(std::unique_ptr<Session> new_session) {
  session = new_session.release();
}
