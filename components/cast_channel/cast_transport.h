// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_TRANSPORT_H_
#define COMPONENTS_CAST_CHANNEL_CAST_TRANSPORT_H_

#include <string>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_checker.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/logger.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace net {
class DrainableIOBuffer;
class DrainableIOBuffer;
class GrowableIOBuffer;
class IOBuffer;
class Socket;
}  // namespace net

namespace cast_channel {

using ::cast::channel::CastMessage;
class MessageFramer;

class CastTransport {
 public:
  virtual ~CastTransport() {}

  // Object to be informed of incoming messages and read errors.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called once Transport is successfully initialized and started.
    // Owned read delegates are Start()ed automatically.
    virtual void Start() = 0;

    // An error occurred on the channel.
    // The caller is responsible for closing |socket| if an error occurred.
    virtual void OnError(ChannelError error_state) = 0;

    // A message was received on the channel.
    virtual void OnMessage(const CastMessage& message) = 0;
  };

  // Sends a CastMessage to |socket_|.
  // |message|: The message to send.
  // |callback|: Callback to be invoked when the write operation has finished.
  // Virtual for testing.
  virtual void SendMessage(const CastMessage& message,
                           net::CompletionOnceCallback callback) = 0;

  // Initializes the reading state machine and starts reading from the
  // underlying socket.
  // Virtual for testing.
  virtual void Start() = 0;

  // Changes the delegate for processing read events. Pending reads remain
  // in-flight.
  // Ownership of the pointee of |delegate| is assumed by the transport.
  // Prior delegates are deleted automatically.
  virtual void SetReadDelegate(std::unique_ptr<Delegate> delegate) = 0;
};

// Manager class for reading and writing messages to/from a socket.
class CastTransportImpl : public CastTransport {
 public:
  using ChannelError = ::cast_channel::ChannelError;

  // Interface to read/write data from a socket to ease unit-testing.
  class Channel {
   public:
    virtual ~Channel() {}
    virtual void Read(net::IOBuffer* buffer,
                      int bytes,
                      net::CompletionOnceCallback callback) = 0;
    virtual void Write(net::IOBuffer* buffer,
                       int bytes,
                       net::CompletionOnceCallback callback) = 0;
  };

  // Adds a CastMessage read/write layer to a socket.
  // Message read events are propagated to the owner via |read_delegate|.
  // |vlog_prefix| sets the prefix used for all VLOGged output.
  // |channel| and |logger| must all out-live the
  // CastTransportImpl instance.
  // |read_delegate| is owned by this CastTransportImpl object.
  CastTransportImpl(Channel* channel,
                    int channel_id,
                    const net::IPEndPoint& ip_endpoint_,
                    scoped_refptr<Logger> logger);

  ~CastTransportImpl() override;

  // CastTransport interface.
  void SendMessage(const CastMessage& message,
                   net::CompletionOnceCallback callback) override;
  void Start() override;
  void SetReadDelegate(std::unique_ptr<Delegate> delegate) override;

 private:
  // Holds a message to be written to the socket. |callback| is invoked when the
  // message is fully written or an error occurrs.
  struct WriteRequest {
    explicit WriteRequest(const std::string& namespace_,
                          const std::string& payload,
                          net::CompletionOnceCallback callback);
    WriteRequest(WriteRequest&& other);
    ~WriteRequest();

    // Namespace of the serialized message.
    std::string message_namespace;
    // Write completion callback, invoked when the operation has completed or
    // failed.
    net::CompletionOnceCallback callback;
    // Buffer with outgoing data.
    scoped_refptr<net::DrainableIOBuffer> io_buffer;
  };

  static bool IsTerminalReadState(ReadState read_state);
  static bool IsTerminalWriteState(WriteState write_state);

  void SetReadState(ReadState read_state);
  void SetWriteState(WriteState write_state);
  void SetErrorState(ChannelError error_state);

  // Terminates all in-flight write callbacks with error code ERR_FAILED.
  void FlushWriteQueue();

  // Main method that performs write flow state transitions.
  void OnWriteResult(int result);

  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is WRITE_STATE_WRITE_COMPLETE
  // DowriteComplete is called, and so on.
  int DoWrite();
  int DoWriteComplete(int result);
  int DoWriteCallback();
  int DoWriteHandleError(int result);

  // Main method that performs write flow state transitions.
  void OnReadResult(int result);

  // Each of the below Do* method is executed in the corresponding
  // write state. For example when read state is READ_STATE_READ_COMPLETE
  // DoReadComplete is called, and so on.
  int DoRead();
  int DoReadComplete(int result);
  int DoReadCallback();
  int DoReadHandleError(int result);

  // Indicates that the transport object is started and may receive and send
  // messages.
  bool started_;

  // Queue of pending writes. The message at the front of the queue is the one
  // being written.
  base::queue<WriteRequest> write_queue_;

  // Buffer used for read operations. Reused for every read.
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  // Constructs and parses the wire representation of message frames.
  std::unique_ptr<MessageFramer> framer_;

  // Last message received on the socket.
  std::unique_ptr<CastMessage> current_message_;

  // Channel used for I/O operations.
  Channel* const channel_;

  // Methods for communicating message receipt and error status to client code.
  std::unique_ptr<Delegate> delegate_;

  // Write flow state machine state.
  WriteState write_state_;

  // Read flow state machine state.
  ReadState read_state_;

  // The last error encountered by the channel.
  ChannelError error_state_;

  // Connection metadata for logging purposes.
  // Socket ID assigned by ApiResourceManager.
  int channel_id_;

  // IP address of the remote end.
  const net::IPEndPoint ip_endpoint_;

  // Accumulates details of events and errors, for debugging purposes.
  scoped_refptr<Logger> logger_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastTransportImpl);
};
}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_TRANSPORT_H_
