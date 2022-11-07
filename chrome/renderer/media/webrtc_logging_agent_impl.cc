// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/webrtc_logging_agent_impl.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"

namespace chrome {
namespace {

constexpr base::TimeDelta kMinTimeSinceLastLogBufferSend =
    base::Milliseconds(100);
constexpr base::TimeDelta kSendLogBufferDelay = base::Milliseconds(200);

// There can be only one registered WebRtcLogMessageDelegate, and so this class
// abstracts away that detail, so that we can set callbacks more than once. It
// also abstracts away the detail of what thread the LogMessage call runs on.
class WebRtcLogMessageDelegateImpl : public blink::WebRtcLogMessageDelegate {
 public:
  static WebRtcLogMessageDelegateImpl* GetInstance() {
    static base::NoDestructor<WebRtcLogMessageDelegateImpl> instance;
    return instance.get();
  }

  void Start(
      base::RepeatingCallback<void(mojom::WebRtcLoggingMessagePtr)> callback) {
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    {
      base::AutoLock locked(lock_);
      task_runner_ = task_runner;
      callback_ = std::move(callback);
    }
    blink::InitWebRtcLogging();
  }

  void Stop() {
    {
      base::AutoLock locked(lock_);
      task_runner_ = nullptr;
      callback_.Reset();
    }
  }

  // blink::WebRtcLogMessageDelegate methods:
  void LogMessage(const std::string& message) override {
    // Called from a random thread.
    auto data = mojom::WebRtcLoggingMessage::New(base::Time::Now(), message);
    {
      base::AutoLock locked(lock_);
      if (callback_)
        task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(callback_, std::move(data)));
    }
  }

  WebRtcLogMessageDelegateImpl() { blink::InitWebRtcLoggingDelegate(this); }

 private:
  ~WebRtcLogMessageDelegateImpl() override = default;

  base::Lock lock_;
  base::RepeatingCallback<void(mojom::WebRtcLoggingMessagePtr)> callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

WebRtcLoggingAgentImpl::WebRtcLoggingAgentImpl() = default;
WebRtcLoggingAgentImpl::~WebRtcLoggingAgentImpl() = default;

void WebRtcLoggingAgentImpl::AddReceiver(
    mojo::PendingReceiver<mojom::WebRtcLoggingAgent> receiver) {
  self_receiver_set_.Add(this, std::move(receiver));
}

void WebRtcLoggingAgentImpl::Start(
    mojo::PendingRemote<mojom::WebRtcLoggingClient> pending_client) {
  // We only support one client at a time. OK to drop any existing client.
  client_.reset();
  client_.Bind(std::move(pending_client));

  WebRtcLogMessageDelegateImpl::GetInstance()->Start(base::BindRepeating(
      &WebRtcLoggingAgentImpl::OnNewMessage, weak_factory_.GetWeakPtr()));
}

void WebRtcLoggingAgentImpl::Stop() {
  if (!log_buffer_.empty())
    SendLogBuffer();
  WebRtcLogMessageDelegateImpl::GetInstance()->Stop();
  if (client_) {
    client_->OnStopped();
    client_.reset();
  }
}

void WebRtcLoggingAgentImpl::OnNewMessage(
    mojom::WebRtcLoggingMessagePtr message) {
  // We may have already been asked to stop.
  if (!client_)
    return;

  log_buffer_.emplace_back(std::move(message));
  if (log_buffer_.size() > 1) {
    // A delayed task has already been posted for sending the buffer contents.
    return;
  }

  if ((base::TimeTicks::Now() - last_log_buffer_send_) >
      kMinTimeSinceLastLogBufferSend) {
    SendLogBuffer();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebRtcLoggingAgentImpl::SendLogBuffer,
                       weak_factory_.GetWeakPtr()),
        kSendLogBufferDelay);
  }
}

void WebRtcLoggingAgentImpl::SendLogBuffer() {
  last_log_buffer_send_ = base::TimeTicks::Now();
  if (client_) {
    client_->OnAddMessages(std::move(log_buffer_));
  } else {
    log_buffer_.clear();
  }
}

}  // namespace chrome
