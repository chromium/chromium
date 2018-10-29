// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_SMALL_MESSAGE_SOCKET_H_
#define CHROMECAST_NET_SMALL_MESSAGE_SOCKET_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class IOBuffer;
class Socket;
}  // namespace net

namespace chromecast {

// Sends and receives small messages (< 64 KB) over a Socket. All methods must
// be called on the same sequence. Any of the virtual methods can destroy this
// object if desired.
class SmallMessageSocket {
 public:
  explicit SmallMessageSocket(std::unique_ptr<net::Socket> socket);
  virtual ~SmallMessageSocket();

  const net::Socket* socket() const { return socket_.get(); }

  // Prepares a buffer to send a message of the given |message_size|. Returns
  // nullptr if sending is not allowed right now (ie, another send is currently
  // in progress). Otherwise, returns a buffer at least large enough to contain
  // |message_size| bytes. The caller should fill in the buffer as desired and
  // then call Send() to send the finished message.
  // If nullptr is returned, then OnSendUnblocked() will be called once sending
  // is possible again.
  void* PrepareSend(int message_size);
  void Send();

  // Sends an already-prepared buffer of data, if possible. The first 2 bytes of
  // the buffer must contain the size of the rest of the data, encoded as a
  // 16-bit integer in big-endian byte order. Returns true if the buffer will be
  // sent; returns false if sending is not allowed right now (ie, another send
  // is currently in progress). If false is returned, then OnSendUnblocked()
  // will be called once sending is possible again.
  bool SendBuffer(net::IOBuffer* data, int size);

  // Enables receiving messages from the stream. Messages will be received and
  // passed to OnMessage() until either an error occurs, the end of stream is
  // reached, or OnMessage() returns false. If OnMessage() returns false, you
  // may call ReceiveMessages() to start receiving again.
  void ReceiveMessages();

 protected:
  // Called when sending becomes possible again, if a previous attempt to send
  // was rejected.
  virtual void OnSendUnblocked() {}

  // Called when an unrecoverable error occurs while sending or receiving. Is
  // only called asynchronously.
  virtual void OnError(int error) {}

  // Called when the end of stream has been read. No more data will be received.
  virtual void OnEndOfStream() {}

  // Called when a message has been received. The |data| buffer contains |size|
  // bytes of data.
  virtual bool OnMessage(char* data, int size) = 0;

 private:
  void OnWriteComplete(int result);
  bool HandleWriteResult(int result);
  void PostError(int error);

  void StartReading();
  void Read();
  void OnReadComplete(int result);
  bool HandleReadResult(int result);
  bool HandleCompletedMessages();

  std::unique_ptr<net::Socket> socket_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  scoped_refptr<net::GrowableIOBuffer> write_storage_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  bool send_blocked_ = false;

  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  base::WeakPtrFactory<SmallMessageSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SmallMessageSocket);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_SMALL_MESSAGE_SOCKET_H_
