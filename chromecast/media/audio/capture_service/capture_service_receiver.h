// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CAPTURE_SERVICE_RECEIVER_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CAPTURE_SERVICE_RECEIVER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/audio/capture_service/constants.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
namespace media {

class CaptureServiceReceiver {
 public:
  // Delegate cannot be destroyed before CaptureServiceReceiver. Note methods of
  // Delegate will always be called from CaptureServiceReceiver's own thread.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual bool OnInitialStreamInfo(
        const capture_service::StreamInfo& stream_info) = 0;

    // Called when more data are received from socket. Return |true| to continue
    // reading messages after OnCaptureData() returns.
    virtual bool OnCaptureData(const char* data, size_t size) = 0;

    // Called when internal error occurs.
    virtual void OnCaptureError() = 0;

    // Called when there is metadata.
    virtual void OnCaptureMetadata(const char* data, size_t size) = 0;
  };

  // The timeout for a connecting socket to stop waiting and report error.
  static constexpr base::TimeDelta kConnectTimeout = base::Seconds(1);

  CaptureServiceReceiver(capture_service::StreamInfo request_stream_info,
                         Delegate* delegate);
  CaptureServiceReceiver(const CaptureServiceReceiver&) = delete;
  CaptureServiceReceiver& operator=(const CaptureServiceReceiver&) = delete;
  ~CaptureServiceReceiver();

  // Starts capture receiving process. It cannot be called if
  // CaptureServiceReceiver has been started and not stopped.
  void Start();
  // Stops capture receiving process synchronously. It will be no-op if Start()
  // has not been started or already been stopped.
  void Stop();

  // Unit test can call this method rather than the public one to inject mocked
  // stream socket.
  void StartWithSocket(std::unique_ptr<net::StreamSocket> connecting_socket);

  // Unit test can set test task runner so as to run test in sync. Must be
  // called after construction and before any other methods.
  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  class Socket;

  void OnConnected(int result);
  void OnConnectTimeout();
  void StopOnTaskRunner(base::WaitableEvent* finished);

  const capture_service::StreamInfo request_stream_info_;
  Delegate* const delegate_;

  // Socket requires IO thread, and low latency input stream requires high
  // thread priority. Therefore, a private thread instead of the IO thread from
  // browser thread pool is necessary so as to make sure input stream won't be
  // blocked nor block other tasks on the same thread.
  base::Thread io_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<net::StreamSocket> connecting_socket_;
  std::unique_ptr<Socket> socket_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CAPTURE_SERVICE_RECEIVER_H_
