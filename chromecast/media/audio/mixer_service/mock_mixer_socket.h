// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MOCK_MIXER_SOCKET_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MOCK_MIXER_SOCKET_H_
#include "chromecast/media/audio/mixer_service/mixer_socket.h"

#include "base/task/sequenced_task_runner.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace mixer_service {

class MockMixerSocket : public MixerSocket {
 public:
  MockMixerSocket();
  ~MockMixerSocket() override;

  MOCK_METHOD(void,
              SetLocalCounterpart,
              (base::WeakPtr<AudioSocket>,
               scoped_refptr<base::SequencedTaskRunner>),
              (override));
  MOCK_METHOD(base::WeakPtr<AudioSocket>,
              GetAudioSocketWeakPtr,
              (),
              (override));
  MOCK_METHOD(void, SetDelegate, (MixerSocket::Delegate*), (override));
  MOCK_METHOD(void, UseBufferPool, (scoped_refptr<IOBufferPool>), (override));
  MOCK_METHOD(bool,
              SendAudioBuffer,
              (scoped_refptr<net::IOBuffer>, int, int64_t),
              (override));
  MOCK_METHOD(bool,
              SendProto,
              (int, const google::protobuf::MessageLite& message),
              (override));
  MOCK_METHOD(void, ReceiveMoreMessages, (), (override));
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MOCK_MIXER_SOCKET_H_
