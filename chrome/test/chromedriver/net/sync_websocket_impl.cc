// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/sync_websocket_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/test/chromedriver/net/command_id.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

SyncWebSocketImpl::SyncWebSocketImpl(
    net::URLRequestContextGetter* context_getter)
    : core_(new Core(context_getter)) {
  core_->SetNotificationCallback(base::BindRepeating(
      &SyncWebSocketImpl::SendNotification, weak_factory_.GetWeakPtr()));
}

SyncWebSocketImpl::~SyncWebSocketImpl() = default;

bool SyncWebSocketImpl::IsConnected() {
  return core_->IsConnected();
}

bool SyncWebSocketImpl::Connect(const GURL& url) {
  return core_->Connect(url);
}

bool SyncWebSocketImpl::Send(const std::string& message) {
  return core_->Send(message);
}

SyncWebSocket::StatusCode SyncWebSocketImpl::ReceiveNextMessage(
    std::string* message, const Timeout& timeout) {
  return core_->ReceiveNextMessage(message, timeout);
}

bool SyncWebSocketImpl::HasNextMessage() {
  return core_->HasNextMessage();
}

void SyncWebSocketImpl::SetNotificationCallback(
    base::RepeatingClosure callback) {
  notify_ = std::move(callback);
}

void SyncWebSocketImpl::SendNotification() {
  if (notify_) {
    notify_.Run();
  }
}

SyncWebSocketImpl::Core::Core(net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter),
      is_connected_(false),
      on_update_event_(&lock_),
      owning_sequence_(base::SequencedTaskRunner::GetCurrentDefault()) {}

bool SyncWebSocketImpl::Core::IsConnected() {
  base::AutoLock lock(lock_);
  return is_connected_;
}

bool SyncWebSocketImpl::Core::Connect(const GURL& url) {
  bool success = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  // Connect with retries. The retry timeout starts at 4 seconds, with
  // exponential backoff, up to 16 seconds. The maximum total wait time is
  // about 30 seconds. (Normally, a successful connection takes only a few
  // milliseconds on Linux and Mac, but around 2 seconds for Windows 10.)
  // See https://crbug.com/chromedriver/3301 for Windows 10 startup times
  const int kMaxTimeout = 16;
  for (int timeout = 4; timeout <= kMaxTimeout; timeout *= 2) {
    context_getter_->GetNetworkTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&SyncWebSocketImpl::Core::ConnectOnIO, this,
                                  url, &success, &event));
    if (event.TimedWait(base::Seconds(timeout)))
      break;
    LOG(WARNING) << "Timed out connecting to Chrome, "
                 << (timeout < kMaxTimeout ? "retrying..." : "giving up.");
  }
  if (!success) {
    // Make sure the underlying connection is closed before we return, otherwise
    // it might try to set event or success flag after they have already gone
    // out of scope.
    context_getter_->GetNetworkTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncWebSocketImpl::Core::CloseOnIO, this, &event));
    event.Wait();
    return false;
  }
  return success;
}

bool SyncWebSocketImpl::Core::Send(const std::string& message) {
  bool success = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  context_getter_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SyncWebSocketImpl::Core::SendOnIO, this,
                                message, &success, &event));
  event.Wait();
  return success;
}

SyncWebSocket::StatusCode SyncWebSocketImpl::Core::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  base::AutoLock lock(lock_);
  while (received_queue_.empty() && is_connected_) {
    base::TimeDelta next_wait = timeout.GetRemainingTime();
    if (next_wait <= base::TimeDelta())
      return SyncWebSocket::StatusCode::kTimeout;
    on_update_event_.TimedWait(next_wait);
  }
  if (!is_connected_)
    return SyncWebSocket::StatusCode::kDisconnected;
  *message = received_queue_.front();
  received_queue_.pop_front();

  return SyncWebSocket::StatusCode::kOk;
}

bool SyncWebSocketImpl::Core::HasNextMessage() {
  base::AutoLock lock(lock_);
  return !received_queue_.empty();
}

// TODO(johnchen) : Send messages with negative command ids to client.
// https://crrev.com/c/1745493 is a pending CL that does this
void SyncWebSocketImpl::Core::OnMessageReceived(const std::string& message) {
  base::AutoLock lock(lock_);

  bool notification_is_needed = false;
  bool send_to_chromedriver;

  DetermineRecipient(message, &send_to_chromedriver);
  if (send_to_chromedriver) {
    notification_is_needed = received_queue_.empty();

    received_queue_.push_back(message);
  }
  on_update_event_.Signal();

  // The notification can be emitted sporadically but we explicitly allow this.
  if (notification_is_needed && notify_) {
    owning_sequence_->PostTask(FROM_HERE, notify_);
  }
}

void SyncWebSocketImpl::Core::DetermineRecipient(const std::string& message,
                                                 bool* send_to_chromedriver) {
  std::optional<base::Value> message_value =
      base::JSONReader::Read(message, base::JSON_REPLACE_INVALID_CHARACTERS);
  base::Value::Dict* message_dict =
      message_value ? message_value->GetIfDict() : nullptr;
  if (!message_dict) {
    *send_to_chromedriver = true;
    return;
  }
  base::Value* id = message_dict->Find("id");
  *send_to_chromedriver =
      id == nullptr ||
      (id->is_int() && CommandId::IsChromeDriverCommandId(id->GetInt()));
}

void SyncWebSocketImpl::Core::OnClose() {
  base::AutoLock lock(lock_);
  is_connected_ = false;
  on_update_event_.Signal();
}

void SyncWebSocketImpl::Core::SetNotificationCallback(
    base::RepeatingClosure callback) {
  base::AutoLock lock(lock_);
  notify_ = std::move(callback);
}

SyncWebSocketImpl::Core::~Core() = default;

void SyncWebSocketImpl::Core::ConnectOnIO(
    const GURL& url,
    bool* success,
    base::WaitableEvent* event) {
  {
    base::AutoLock lock(lock_);
    received_queue_.clear();
  }
  // If this is a retry to connect, there is a chance that the original attempt
  // to connect has succeeded after the retry was initiated, so double check if
  // we are already connected. The is_connected_ flag is only set on the I/O
  // thread, so no additional synchronization is needed to check it here.
  // Note: If is_connected_ is true, both |success| and |event| may point to
  // stale memory, so don't use either parameters before returning.
  if (socket_ && is_connected_)
    return;
  socket_ = std::make_unique<WebSocket>(url, this);
  socket_->Connect(base::BindOnce(
      &SyncWebSocketImpl::Core::OnConnectCompletedOnIO, this, success, event));
}

void SyncWebSocketImpl::Core::OnConnectCompletedOnIO(
    bool* success,
    base::WaitableEvent* event,
    int error) {
  *success = (error == net::OK);
  if (*success) {
    base::AutoLock lock(lock_);
    is_connected_ = true;
  }
  event->Signal();
}

void SyncWebSocketImpl::Core::SendOnIO(
    const std::string& message,
    bool* success,
    base::WaitableEvent* event) {
  *success = socket_->Send(message);
  event->Signal();
}

void SyncWebSocketImpl::Core::OnDestruct() const {
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner =
      context_getter_->GetNetworkTaskRunner();
  if (network_task_runner->BelongsToCurrentThread())
    delete this;
  else
    network_task_runner->DeleteSoon(FROM_HERE, this);
}

void SyncWebSocketImpl::Core::CloseOnIO(base::WaitableEvent* event) {
  socket_.reset();
  event->Signal();
}
