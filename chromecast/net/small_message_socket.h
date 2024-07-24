// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_SMALL_MESSAGE_SOCKET_H_
#define CHROMECAST_NET_SMALL_MESSAGE_SOCKET_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net {
class Socket;
}  // namespace net

namespace chromecast {
class IOBufferPool;

// Sends messages over a Socket. All methods must be called on the same
// sequence. Any of the delegate methods can destroy this object if desired.
class SmallMessageSocket {
 public:
  class Delegate {
   public:
    // Called when sending becomes possible again, if a previous attempt to send
    // was rejected.
    virtual void OnSendUnblocked() {}

    // Called when an unrecoverable error occurs while sending or receiving. Is
    // only called asynchronously.
    virtual void OnError(int error) {}

    // Called when the end of stream has been read. No more data will be
    // received.
    virtual void OnEndOfStream() {}

    // Called when a message has been received and there is no buffer pool. The
    // |data| buffer contains |size| bytes of data. Return |true| to continue
    // reading messages after OnMessage() returns.
    virtual bool OnMessage(char* data, size_t size) = 0;

    // Called when a message has been received. The |buffer| contains |size|
    // bytes of data, which includes the first 2 (or 6) bytes which are the size
    // in network byte order. Note that these 2/6 bytes are not included in
    // OnMessage()! Return |true| to continue receiving messages.
    // Note: if the first 2 bytes are 0xffff, then the following 4 bytes are the
    // size in network byte order (in which case, the offset to the message data
    // is 6 bytes).
    virtual bool OnMessageBuffer(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size);

   protected:
    virtual ~Delegate() = default;
  };

  SmallMessageSocket(Delegate* delegate, std::unique_ptr<net::Socket> socket);

  SmallMessageSocket(const SmallMessageSocket&) = delete;
  SmallMessageSocket& operator=(const SmallMessageSocket&) = delete;

  virtual ~SmallMessageSocket();

  net::Socket* socket() const { return socket_.get(); }
  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }
  IOBufferPool* buffer_pool() const { return buffer_pool_.get(); }

  // Adds a |buffer_pool| used to allocate buffers to receive messages into;
  // received messages are passed to OnMessageBuffer(). If a message would be
  // too big to fit in a pool-provided buffer, a dynamically allocated IOBuffer
  // will be used instead for that message.
  void UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool);

  // Removes the buffer pool; subsequent received messages will be passed to
  // OnMessage().
  void RemoveBufferPool();

  // Prepares a buffer to send a message of the given |message_size|. Returns
  // nullptr if sending is not allowed right now (ie, another send is currently
  // in progress). Otherwise, returns a buffer at least large enough to contain
  // |message_size| bytes. The caller should fill in the buffer as desired and
  // then call Send() to send the finished message.
  // If nullptr is returned, then OnSendUnblocked() will be called once sending
  // is possible again.
  //
  // TODO(crbug.com/40284755): This method should return a span of size
  // `message_size` instead of a pointer.
  void* PrepareSend(size_t message_size);
  void Send();

  // Sends an already-prepared buffer of data, if possible. The first part of
  // the buffer should contain the message size information as written by
  // WriteSizeData(). Returns true if the buffer will be sent; returns false if
  // sending is not allowed right now (ie, another send is currently in
  // progress). If false is returned, then OnSendUnblocked() will be called once
  // sending is possible again.
  bool SendBuffer(scoped_refptr<net::IOBuffer> data, size_t size);

  // Returns the number of bytes used for size information for the given message
  // size.
  static size_t SizeDataBytes(size_t message_size);

  // Writes the necessary |message_size| information into `buf`. This can be
  // used to prepare a buffer for SendBuffer(). Note that if `message_size` is
  // greater than or equal to 0xffff, the message size data will take up 6
  // bytes, and the `buf` will need to be large enough. Returns the number of
  // bytes written (= SizeDataBytes(message_size)).
  static size_t WriteSizeData(base::span<uint8_t> buf, size_t message_size);

  // Reads the message size from a |ptr| which contains |bytes_read| of data.
  // Returns |false| if there was not enough data to read a valid size.
  static bool ReadSize(char* ptr,
                       size_t bytes_read,
                       size_t& data_offset,
                       size_t& message_size);

  // Enables receiving messages from the stream. Messages will be received and
  // passed to OnMessage() until either an error occurs, the end of stream is
  // reached, or OnMessage() returns false. If OnMessage() returns false, you
  // may call ReceiveMessages() to start receiving again. OnMessage() will not
  // be called synchronously from within this method (it always posts a task).
  void ReceiveMessages();

  // Same as ReceiveMessages(), but OnMessage() may be called synchronously.
  // This is more efficient because it doesn't post a task to ensure
  // asynchronous reads.
  void ReceiveMessagesSynchronously();

 private:
  friend class SmallMessageSocketTest;

  // This class wraps the IOBuffer and controls its range to point into
  // `buffer_` but allowing it to be a subset of `buffer_` that can shrink as
  // bytes are consumed from the front. The base IOBuffer is passed in from
  // SetUnderlyingBuffer.
  class BufferWrapper : public net::IOBuffer {
   public:
    BufferWrapper();

    // Set the base buffer. `capacity` is the total size of `base`.
    void SetUnderlyingBuffer(scoped_refptr<net::IOBuffer> base,
                             size_t capacity);
    scoped_refptr<net::IOBuffer> TakeUnderlyingBuffer();
    void ClearUnderlyingBuffer();

    // Offset the next available bytes in the buffer.
    void DidConsume(size_t bytes);

    // A pointer to the very start of the buffer. The `span()` returned will
    // move as DidConsume() is called and moves the start of the buffer forward.
    // But this will always return the absolute beginning of the buffer.
    // TODO(328018028): This should return a span.
    char* StartOfBuffer() const;

    size_t used() const { return used_; }
    size_t capacity() const { return capacity_; }

    base::span<const uint8_t> used_span() const;

   private:
    ~BufferWrapper() override;

    // The buffer that actually holds the memory.
    scoped_refptr<net::IOBuffer> buffer_;

    // Size of the used bytes in previous operations.
    size_t used_ = 0;

    // Total size of `buffer_`.
    size_t capacity_ = 0;
  };

  void OnWriteComplete(int result);
  bool HandleWriteResult(int result);
  void OnError(int error);

  void Read();
  void OnReadComplete(int result);
  bool HandleReadResult(int result);
  bool HandleCompletedMessages();
  bool HandleCompletedMessageBuffers();
  void ActivateBufferPool(base::span<const uint8_t> current_data);

  Delegate* const delegate_;
  const std::unique_ptr<net::Socket> socket_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  const scoped_refptr<net::GrowableIOBuffer> write_storage_;
  const scoped_refptr<BufferWrapper> write_buffer_;
  bool send_blocked_ = false;

  const scoped_refptr<net::GrowableIOBuffer> read_storage_;

  scoped_refptr<IOBufferPool> buffer_pool_;
  const scoped_refptr<BufferWrapper> read_buffer_;

  bool in_message_ = false;

  base::WeakPtrFactory<SmallMessageSocket> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_SMALL_MESSAGE_SOCKET_H_
