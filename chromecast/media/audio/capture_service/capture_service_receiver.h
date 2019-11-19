// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CAPTURE_SERVICE_RECEIVER_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CAPTURE_SERVICE_RECEIVER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

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
  // The timeout for a connecting socket to stop waiting and report error.
  static constexpr base::TimeDelta kConnectTimeout =
      base::TimeDelta::FromSeconds(1);

  explicit CaptureServiceReceiver(const ::media::AudioParameters& audio_params);
  ~CaptureServiceReceiver();

  void Start(::media::AudioInputStream::AudioInputCallback* input_callback);
  void Stop();

  // Unit test can call this method rather than the public one to inject mocked
  // stream socket.
  void StartWithSocket(
      ::media::AudioInputStream::AudioInputCallback* input_callback,
      std::unique_ptr<net::StreamSocket> connecting_socket);

  // Unit test can set test task runner so as to run test in sync. Must be
  // called after construction and before any other methods.
  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  class Socket;

  void OnConnected(
      ::media::AudioInputStream::AudioInputCallback* input_callback,
      int result);
  void OnConnectTimeout(
      ::media::AudioInputStream::AudioInputCallback* input_callback);
  void StopOnTaskRunner(base::WaitableEvent* finished);

  const ::media::AudioParameters audio_params_;

  // Socket requires IO thread, and low latency input stream requires high
  // thread priority. Therefore, a private thread instead of the IO thread from
  // browser thread pool is necessary so as to make sure input stream won't be
  // blocked nor block other tasks on the same thread.
  base::Thread io_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<net::StreamSocket> connecting_socket_;
  std::unique_ptr<Socket> socket_;

  DISALLOW_COPY_AND_ASSIGN(CaptureServiceReceiver);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CAPTURE_SERVICE_RECEIVER_H_
