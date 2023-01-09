// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_

#include <cstdint>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/media/audio/net/audio_socket.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {
class IOBufferPool;

namespace media {
namespace mixer_service {
class Generic;

class MixerSocket {
 public:
  static constexpr int kAudioMessageHeaderSize =
      AudioSocket::kAudioMessageHeaderSize;

  class Delegate : public AudioSocket::Delegate {
   public:
    // Called when metadata is received from the other side of the connection.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleMetadata(const Generic& message);

   protected:
    ~Delegate() override = default;
  };

  virtual ~MixerSocket() = default;

  virtual void SetLocalCounterpart(
      base::WeakPtr<AudioSocket> local_counterpart,
      scoped_refptr<base::SequencedTaskRunner> counterpart_task_runner) = 0;
  virtual base::WeakPtr<AudioSocket> GetAudioSocketWeakPtr() = 0;

  // Sets/changes the delegate. Must be called immediately after creation
  // (ie, synchronously on the same sequence).
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Adds a |buffer_pool| used to allocate buffers to receive messages into,
  // and for sending protos. If the pool-allocated buffers are too small for a
  // given message, a normal IOBuffer will be dynamically allocated instead.
  virtual void UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool) = 0;

  // Prepares |audio_buffer| and then sends it across the connection. Returns
  // |false| if the audio could not be sent.
  virtual bool SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                               int filled_bytes,
                               int64_t timestamp) = 0;

  // Sends an arbitrary protobuf across the connection. |type| indicates the
  // type of message; if the write cannot complete immediately, one message of
  // each type will be stored for later sending; if a newer message is sent with
  // the same type, then the previous message is overwritten. When writes become
  // available again, the stored messages are written in order of |type| (lowest
  // type first). Note that |type| is completely determined by the caller, and
  // you can reuse the same type value for different messages as long as they
  // are on different socket instances. A type of 0 means to never store the
  // message. Returns |false| if the message was not sent or stored.
  virtual bool SendProto(int type,
                         const google::protobuf::MessageLite& message) = 0;

  // Resumes receiving messages. Delegate calls may be called synchronously
  // from within this method.
  virtual void ReceiveMoreMessages() = 0;
};

// AudioSocket implementation for sending and receiving messages to/from the
// mixer service.
class MixerSocketImpl : public MixerSocket {
 public:
  explicit MixerSocketImpl(std::unique_ptr<net::StreamSocket> socket);
  MixerSocketImpl(const MixerSocketImpl&) = delete;
  MixerSocketImpl& operator=(const MixerSocketImpl&) = delete;
  ~MixerSocketImpl() override;

  // Used to create local (in-process) connections.
  MixerSocketImpl();
  void SetLocalCounterpart(base::WeakPtr<AudioSocket> local_counterpart,
                           scoped_refptr<base::SequencedTaskRunner>
                               counterpart_task_runner) override;
  base::WeakPtr<AudioSocket> GetAudioSocketWeakPtr() override;

  void SetDelegate(Delegate* delegate) override;

  void UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool) override;

  bool SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                       int filled_bytes,
                       int64_t timestamp) override;
  bool SendProto(int type,
                 const google::protobuf::MessageLite& message) override;
  void ReceiveMoreMessages() override;

 private:
  class AudioSocketExtension : public AudioSocket {
   public:
    explicit AudioSocketExtension(std::unique_ptr<net::StreamSocket> socket);
    AudioSocketExtension(const MixerSocketImpl&) = delete;
    AudioSocketExtension& operator=(const MixerSocketImpl&) = delete;
    ~AudioSocketExtension() override;

    // Used to create local (in-process) connections.
    AudioSocketExtension();

    void SetDelegate(MixerSocket::Delegate* delegate);

   private:
    bool ParseMetadata(char* data, size_t size) override;

    MixerSocket::Delegate* delegate_ = nullptr;
  };

  std::unique_ptr<AudioSocketExtension> audio_socket_ = nullptr;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_MIXER_SOCKET_H_
