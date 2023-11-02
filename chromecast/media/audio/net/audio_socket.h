// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_H_
#define CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/net/small_message_socket.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace net {
class IOBuffer;
class StreamSocket;
}  // namespace net

namespace chromecast {
class IOBufferPool;

namespace media {

// Base class for sending and receiving messages to/from audio services (e.g.
// mixer service, audio output service).
// Not thread-safe; all usage of a given instance must be on the same IO
// sequence.
class AudioSocket : public SmallMessageSocket::Delegate {
 public:
  class Delegate {
   public:
    // Called when audio data is received from the other side of the connection.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleAudioData(char* data, size_t size, int64_t timestamp);

    // Called when audio data is received from the other side of the connection
    // using an IOBufferPool. The |buffer| reference may be held as long as
    // needed by the delegate implementation. The buffer contains the full
    // message header including size; the |data| points to the audio data, and
    // the |size| is the size of the audio data. |data| will always be
    // kAudioMessageHeaderSize bytes past the start of the buffer.
    // Return |true| if the socket should continue to receive messages.
    virtual bool HandleAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                                   char* data,
                                   size_t size,
                                   int64_t timestamp);

    // Called when the connection is lost; no further data will be sent or
    // received after OnConnectionError() is called. It is safe to delete the
    // AudioSocket inside the OnConnectionError() implementation.
    virtual void OnConnectionError() {}

   protected:
    virtual ~Delegate() = default;
  };

  explicit AudioSocket(std::unique_ptr<net::StreamSocket> socket);
  AudioSocket(const AudioSocket&) = delete;
  AudioSocket& operator=(const AudioSocket&) = delete;
  ~AudioSocket() override;

  // Used to create local (in-process) connections.
  AudioSocket();
  void SetLocalCounterpart(
      base::WeakPtr<AudioSocket> local_counterpart,
      scoped_refptr<base::SequencedTaskRunner> counterpart_task_runner);
  base::WeakPtr<AudioSocket> GetWeakPtr();

  // Sets/changes the delegate. Must be called immediately after creation
  // (ie, synchronously on the same sequence).
  void SetDelegate(Delegate* delegate);

  // Adds a |buffer_pool| used to allocate buffers to receive messages into,
  // and for sending protos. If the pool-allocated buffers are too small for a
  // given message, a normal IOBuffer will be dynamically allocated instead.
  void UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool);

  // 16-bit type and 64-bit timestamp, plus 32-bit padding to align to 16 bytes.
  static constexpr size_t kAudioHeaderSize =
      sizeof(int16_t) + sizeof(int64_t) + sizeof(int32_t);
  // Includes additional 16-bit size field for SmallMessageSocket.
  static constexpr size_t kAudioMessageHeaderSize =
      sizeof(uint16_t) + kAudioHeaderSize;

  // Fills in the audio message header for |buffer|, so it can later be sent via
  // SendPreparedAudioBuffer(). |buffer| should have |kAudioMessageHeaderSize|
  // bytes reserved at the start of the buffer, followed by |filled_bytes| of
  // audio data.
  static void PrepareAudioBuffer(net::IOBuffer* buffer,
                                 int filled_bytes,
                                 int64_t timestamp);

  // Prepares |audio_buffer| and then sends it across the connection. Returns
  // |false| if the audio could not be sent.
  bool SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                       int filled_bytes,
                       int64_t timestamp);

  // Sends |audio_buffer| across the connection. |audio_buffer| should have
  // previously been prepared using PrepareAudioBuffer(). Returns |false| if the
  // audio could not be sent.
  bool SendPreparedAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer);

  // Sends an arbitrary protobuf across the connection. |type| indicates the
  // type of message; if the write cannot complete immediately, one message of
  // each type will be stored for later sending; if a newer message is sent with
  // the same type, then the previous message is overwritten. When writes become
  // available again, the stored messages are written in order of |type| (lowest
  // type first). Note that |type| is completely determined by the caller, and
  // you can reuse the same type value for different messages as long as they
  // are on different socket instances. A type of 0 means to never store the
  // message. Returns |false| if the message was not sent or stored.
  bool SendProto(int type, const google::protobuf::MessageLite& message);

  // Resumes receiving messages. Delegate calls may be called synchronously
  // from within this method.
  void ReceiveMoreMessages();

 private:
  // Parses the meta data received from the connection.
  virtual bool ParseMetadata(char* data, size_t size) = 0;

  bool SendBuffer(int type,
                  scoped_refptr<net::IOBuffer> buffer,
                  size_t buffer_size);
  bool SendBufferToSocket(int type,
                          scoped_refptr<net::IOBuffer> buffer,
                          size_t buffer_size);

  // SmallMessageSocket::Delegate implementation:
  void OnSendUnblocked() override;
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, size_t size) override;
  bool OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer,
                       size_t size) override;

  bool ParseAudio(char* data, size_t size);
  bool ParseAudioBuffer(scoped_refptr<net::IOBuffer> buffer,
                        char* data,
                        size_t size);

  Delegate* delegate_ = nullptr;
  const std::unique_ptr<SmallMessageSocket> socket_;

  scoped_refptr<IOBufferPool> buffer_pool_;
  base::flat_map<int, scoped_refptr<net::IOBuffer>> pending_writes_;

  base::WeakPtr<AudioSocket> local_counterpart_;
  scoped_refptr<base::SequencedTaskRunner> counterpart_task_runner_;

  base::WeakPtrFactory<AudioSocket> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_NET_AUDIO_SOCKET_H_
