// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GRPC_SUPPORT_BIDIRECTIONAL_STREAM_H_
#define COMPONENTS_GRPC_SUPPORT_BIDIRECTIONAL_STREAM_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "net/http/bidirectional_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class Location;
}  // namespace base

namespace net {
class HttpRequestHeaders;
class WrappedIOBuffer;
}  // namespace net

namespace grpc_support {

// An adapter to net::BidirectionalStream.
// Created and configured from any thread. Start, ReadData, WriteData and
// Destroy can be called on any thread (including network thread), and post
// calls to corresponding {Start|ReadData|WriteData|Destroy}OnNetworkThread to
// the network thread. The object is always deleted on network thread. All
// callbacks into the Delegate are done on the network thread.
// The app is expected to initiate the next step like ReadData or Destroy.
// Public methods can be called on any thread.
class BidirectionalStream : public net::BidirectionalStream::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnStreamReady() = 0;

    virtual void OnHeadersReceived(
        const quiche::HttpHeaderBlock& response_headers,
        const char* negotiated_protocol) = 0;

    virtual void OnDataRead(char* data, int size) = 0;

    virtual void OnDataSent(const char* data) = 0;

    virtual void OnTrailersReceived(
        const quiche::HttpHeaderBlock& trailers) = 0;

    virtual void OnSucceeded() = 0;

    virtual void OnFailed(int error) = 0;

    virtual void OnCanceled() = 0;
  };

  BidirectionalStream(net::URLRequestContextGetter* request_context_getter,
                      Delegate* delegate);

  BidirectionalStream(const BidirectionalStream&) = delete;
  BidirectionalStream& operator=(const BidirectionalStream&) = delete;

  ~BidirectionalStream() override;

  // Disables automatic flushing of each buffer passed to WriteData().
  void disable_auto_flush(bool disable_auto_flush) {
    disable_auto_flush_ = disable_auto_flush;
  }

  // Delays sending request headers until first call to Flush().
  void delay_headers_until_flush(bool delay_headers_until_flush) {
    delay_headers_until_flush_ = delay_headers_until_flush;
  }

  // Validates method and headers, initializes and starts the request. If
  // |end_of_stream| is true, then stream is half-closed after sending header
  // frame and no data is expected to be written.
  // Returns 0 if request is valid and started successfully,
  // Returns -1 if |method| is not valid HTTP method name.
  // Returns position of invalid header value in |headers| if header name is
  // not valid.
  int Start(const char* url,
            int priority,
            const char* method,
            const net::HttpRequestHeaders& headers,
            bool end_of_stream);

  // Reads more data into |buffer| up to |capacity| bytes.
  bool ReadData(char* buffer, int capacity);

  // Writes |count| bytes of data from |buffer|. The |end_of_stream| is
  // passed to remote to indicate end of stream.
  bool WriteData(const char* buffer, int count, bool end_of_stream);

  // Sends buffers passed to WriteData().
  void Flush();

  // Cancels the request. The OnCanceled callback is invoked when request is
  // caneceled, and not other callbacks are invoked afterwards..
  void Cancel();

  // Releases all resources for the request and deletes the object itself.
  void Destroy();

 private:
  // States of BidirectionalStream are tracked in |read_state_| and
  // |write_state_|.
  // The write state is separated as it changes independently of the read state.
  // There is one initial state: NOT_STARTED. There is one normal final state:
  // SUCCESS, reached after READING_DONE and WRITING_DONE. There are two
  // exceptional final states: CANCELED and ERROR, which can be reached from
  // any other non-final state.
  enum State {
    // Initial state, stream not started.
    NOT_STARTED,
    // Stream started, request headers are being sent.
    STARTED,
    // Waiting for ReadData() to be called.
    WAITING_FOR_READ,
    // Reading from the remote, OnDataRead callback will be invoked when done.
    READING,
    // There is no more data to read and stream is half-closed by the remote
    // side.
    READING_DONE,
    // Stream is canceled.
    CANCELED,
    // Error has occured, stream is closed.
    ERR,
    // Reading and writing are done, and the stream is closed successfully.
    SUCCESS,
    // Waiting for Flush() to be called.
    WAITING_FOR_FLUSH,
    // Writing to the remote, callback will be invoked when done.
    WRITING,
    // There is no more data to write and stream is half-closed by the local
    // side.
    WRITING_DONE,
  };

  // Container to hold buffers and sizes of the pending data to be written.
  class WriteBuffers {
   public:
    WriteBuffers();

    WriteBuffers(const WriteBuffers&) = delete;
    WriteBuffers& operator=(const WriteBuffers&) = delete;

    ~WriteBuffers();

    // Clears Write Buffers list.
    void Clear();

    // Appends |buffer| of |buffer_size| length to the end of buffer list.
    void AppendBuffer(const scoped_refptr<net::IOBuffer>& buffer,
                      int buffer_size);

    void MoveTo(WriteBuffers* target);

    // Returns true of Write Buffers list is empty.
    bool Empty() const;

    const std::vector<scoped_refptr<net::IOBuffer>>& buffers() const {
      return write_buffer_list;
    }

    const std::vector<int>& lengths() const { return write_buffer_len_list; }

   private:
    // Every IOBuffer in |write_buffer_list| points to the memory owned by the
    // application.
    std::vector<scoped_refptr<net::IOBuffer>> write_buffer_list;
    // A list of the length of each IOBuffer in |write_buffer_list|.
    std::vector<int> write_buffer_len_list;
  };

  // net::BidirectionalStream::Delegate implementations:
  void OnStreamReady(bool request_headers_sent) override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataRead(int bytes_read) override;
  void OnDataSent() override;
  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override;
  void OnFailed(int error) override;
  // Helper method to derive OnSucceeded.
  void MaybeOnSucceded();

  void StartOnNetworkThread(
      std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info);
  void ReadDataOnNetworkThread(scoped_refptr<net::WrappedIOBuffer> read_buffer,
                               int buffer_size);
  void WriteDataOnNetworkThread(scoped_refptr<net::WrappedIOBuffer> read_buffer,
                                int buffer_size,
                                bool end_of_stream);
  void FlushOnNetworkThread();
  void SendFlushingWriteData();
  void CancelOnNetworkThread();
  void DestroyOnNetworkThread();

  bool IsOnNetworkThread();
  void PostToNetworkThread(const base::Location& from_here,
                           base::OnceClosure task);

  // Read state is tracking reading flow. Only accessed on network thread.
  //                         | <--- READING <--- |
  //                         |                   |
  //                         |                   |
  // NOT_STARTED -> STARTED --> WAITING_FOR_READ -> READING_DONE -> SUCCESS
  State read_state_;

  // Write state is tracking writing flow.  Only accessed on network thread.
  //                         | <--- WRITING <---  |
  //                         |                    |
  //                         |                    |
  // NOT_STARTED -> STARTED --> WAITING_FOR_FLUSH -> WRITING_DONE -> SUCCESS
  State write_state_;

  bool write_end_of_stream_;
  bool request_headers_sent_;

  bool disable_auto_flush_;
  bool delay_headers_until_flush_;

  const raw_ptr<net::URLRequestContextGetter> request_context_getter_;

  scoped_refptr<net::WrappedIOBuffer> read_buffer_;

  // Write data that is pending the flush.
  std::unique_ptr<WriteBuffers> pending_write_data_;
  // Write data that is flushed, but not sending yet.
  std::unique_ptr<WriteBuffers> flushing_write_data_;
  // Write data that is sending.
  std::unique_ptr<WriteBuffers> sending_write_data_;

  std::unique_ptr<net::BidirectionalStream> bidi_stream_;
  raw_ptr<Delegate> delegate_;

  base::WeakPtr<BidirectionalStream> weak_this_;
  base::WeakPtrFactory<BidirectionalStream> weak_factory_{this};
};

}  // namespace grpc_support

#endif  // COMPONENTS_GRPC_SUPPORT_BIDIRECTIONAL_STREAM_H_
