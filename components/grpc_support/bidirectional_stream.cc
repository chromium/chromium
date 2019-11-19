// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/grpc_support/bidirectional_stream.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/bidirectional_stream.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace grpc_support {

BidirectionalStream::WriteBuffers::WriteBuffers() {}

BidirectionalStream::WriteBuffers::~WriteBuffers() {}

void BidirectionalStream::WriteBuffers::Clear() {
  write_buffer_list.clear();
  write_buffer_len_list.clear();
}

void BidirectionalStream::WriteBuffers::AppendBuffer(
    const scoped_refptr<net::IOBuffer>& buffer,
    int buffer_size) {
  write_buffer_list.push_back(buffer);
  write_buffer_len_list.push_back(buffer_size);
}

void BidirectionalStream::WriteBuffers::MoveTo(WriteBuffers* target) {
  std::move(write_buffer_list.begin(), write_buffer_list.end(),
            std::back_inserter(target->write_buffer_list));
  std::move(write_buffer_len_list.begin(), write_buffer_len_list.end(),
            std::back_inserter(target->write_buffer_len_list));
  Clear();
}

bool BidirectionalStream::WriteBuffers::Empty() const {
  return write_buffer_list.empty();
}

BidirectionalStream::BidirectionalStream(
    net::URLRequestContextGetter* request_context_getter,
    Delegate* delegate)
    : read_state_(NOT_STARTED),
      write_state_(NOT_STARTED),
      write_end_of_stream_(false),
      request_headers_sent_(false),
      disable_auto_flush_(false),
      delay_headers_until_flush_(false),
      request_context_getter_(request_context_getter),
      pending_write_data_(new WriteBuffers()),
      flushing_write_data_(new WriteBuffers()),
      sending_write_data_(new WriteBuffers()),
      delegate_(delegate) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

BidirectionalStream::~BidirectionalStream() {
  DCHECK(IsOnNetworkThread());
}

int BidirectionalStream::Start(const char* url,
                               int priority,
                               const char* method,
                               const net::HttpRequestHeaders& headers,
                               bool end_of_stream) {
  // Prepare request info here to be able to return the error.
  std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info(
      new net::BidirectionalStreamRequestInfo());
  request_info->url = GURL(url);
  request_info->priority = static_cast<net::RequestPriority>(priority);
  // Http method is a token, just as header name.
  request_info->method = method;
  if (!net::HttpUtil::IsValidHeaderName(request_info->method))
    return -1;
  request_info->extra_headers.CopyFrom(headers);
  request_info->end_stream_on_headers = end_of_stream;
  write_end_of_stream_ = end_of_stream;
  PostToNetworkThread(FROM_HERE,
                      base::BindOnce(&BidirectionalStream::StartOnNetworkThread,
                                     weak_this_, std::move(request_info)));
  return 0;
}

bool BidirectionalStream::ReadData(char* buffer, int capacity) {
  if (!buffer)
    return false;
  scoped_refptr<net::WrappedIOBuffer> read_buffer =
      base::MakeRefCounted<net::WrappedIOBuffer>(buffer);

  PostToNetworkThread(
      FROM_HERE, base::BindOnce(&BidirectionalStream::ReadDataOnNetworkThread,
                                weak_this_, std::move(read_buffer), capacity));
  return true;
}

bool BidirectionalStream::WriteData(const char* buffer,
                                    int count,
                                    bool end_of_stream) {
  if (!buffer)
    return false;

  scoped_refptr<net::WrappedIOBuffer> write_buffer =
      base::MakeRefCounted<net::WrappedIOBuffer>(buffer);

  PostToNetworkThread(
      FROM_HERE,
      base::BindOnce(&BidirectionalStream::WriteDataOnNetworkThread, weak_this_,
                     std::move(write_buffer), count, end_of_stream));

  return true;
}

void BidirectionalStream::Flush() {
  PostToNetworkThread(
      FROM_HERE,
      base::BindOnce(&BidirectionalStream::FlushOnNetworkThread, weak_this_));
}

void BidirectionalStream::Cancel() {
  PostToNetworkThread(
      FROM_HERE,
      base::BindOnce(&BidirectionalStream::CancelOnNetworkThread, weak_this_));
}

void BidirectionalStream::Destroy() {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete.
  PostToNetworkThread(
      FROM_HERE, base::BindOnce(&BidirectionalStream::DestroyOnNetworkThread,
                                base::Unretained(this)));
}

void BidirectionalStream::OnStreamReady(bool request_headers_sent) {
  DCHECK(IsOnNetworkThread());
  DCHECK_EQ(STARTED, write_state_);
  if (!bidi_stream_)
    return;
  request_headers_sent_ = request_headers_sent;
  write_state_ = WAITING_FOR_FLUSH;
  if (write_end_of_stream_) {
    if (!request_headers_sent) {
      // If there is no data to write, then just send headers explicitly.
      bidi_stream_->SendRequestHeaders();
      request_headers_sent_ = true;
    }
    write_state_ = WRITING_DONE;
  }
  delegate_->OnStreamReady();
}

void BidirectionalStream::OnHeadersReceived(
    const spdy::SpdyHeaderBlock& response_headers) {
  DCHECK(IsOnNetworkThread());
  DCHECK_EQ(STARTED, read_state_);
  if (!bidi_stream_)
    return;
  read_state_ = WAITING_FOR_READ;
  // Get http status code from response headers.
  int http_status_code = 0;
  const auto http_status_header = response_headers.find(":status");
  if (http_status_header != response_headers.end())
    base::StringToInt(http_status_header->second, &http_status_code);
  const char* protocol = "unknown";
  switch (bidi_stream_->GetProtocol()) {
    case net::kProtoHTTP2:
      protocol = "h2";
      break;
    case net::kProtoQUIC:
      protocol = "quic/1+spdy/3";
      break;
    default:
      break;
  }
  delegate_->OnHeadersReceived(response_headers, protocol);
}

void BidirectionalStream::OnDataRead(int bytes_read) {
  DCHECK(IsOnNetworkThread());
  DCHECK_EQ(READING, read_state_);
  if (!bidi_stream_)
    return;
  read_state_ = WAITING_FOR_READ;
  delegate_->OnDataRead(read_buffer_->data(), bytes_read);

  // Free the read buffer.
  read_buffer_ = nullptr;
  if (bytes_read == 0)
    read_state_ = READING_DONE;
  MaybeOnSucceded();
}

void BidirectionalStream::OnDataSent() {
  DCHECK(IsOnNetworkThread());
  if (!bidi_stream_)
    return;
  DCHECK_EQ(WRITING, write_state_);
  write_state_ = WAITING_FOR_FLUSH;
  for (const scoped_refptr<net::IOBuffer>& buffer :
       sending_write_data_->buffers()) {
    delegate_->OnDataSent(buffer->data());
  }
  sending_write_data_->Clear();
  // Send data flushed while other data was sending.
  if (!flushing_write_data_->Empty()) {
    SendFlushingWriteData();
    return;
  }
  if (write_end_of_stream_ && pending_write_data_->Empty()) {
    write_state_ = WRITING_DONE;
    MaybeOnSucceded();
  }
}

void BidirectionalStream::OnTrailersReceived(
    const spdy::SpdyHeaderBlock& response_trailers) {
  DCHECK(IsOnNetworkThread());
  if (!bidi_stream_)
    return;
  delegate_->OnTrailersReceived(response_trailers);
}

void BidirectionalStream::OnFailed(int error) {
  DCHECK(IsOnNetworkThread());
  if (!bidi_stream_ && read_state_ != NOT_STARTED)
    return;
  read_state_ = write_state_ = ERR;
  weak_factory_.InvalidateWeakPtrs();
  // Delete underlying |bidi_stream_| asynchronously as it may still be used.
  PostToNetworkThread(
      FROM_HERE, base::BindOnce(&base::DeletePointer<net::BidirectionalStream>,
                                bidi_stream_.release()));
  delegate_->OnFailed(error);
}

void BidirectionalStream::StartOnNetworkThread(
    std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info) {
  DCHECK(IsOnNetworkThread());
  DCHECK(!bidi_stream_);
  DCHECK(request_context_getter_->GetURLRequestContext());
  net::URLRequestContext* request_context =
      request_context_getter_->GetURLRequestContext();
  request_info->extra_headers.SetHeaderIfMissing(
      net::HttpRequestHeaders::kUserAgent,
      request_context->http_user_agent_settings()->GetUserAgent());
  bidi_stream_.reset(new net::BidirectionalStream(
      std::move(request_info),
      request_context->http_transaction_factory()->GetSession(),
      !delay_headers_until_flush_, this));
  DCHECK(read_state_ == NOT_STARTED && write_state_ == NOT_STARTED);
  read_state_ = write_state_ = STARTED;
}

void BidirectionalStream::ReadDataOnNetworkThread(
    scoped_refptr<net::WrappedIOBuffer> read_buffer,
    int buffer_size) {
  DCHECK(IsOnNetworkThread());
  DCHECK(read_buffer);
  DCHECK(!read_buffer_);
  if (read_state_ != WAITING_FOR_READ) {
    DLOG(ERROR) << "Unexpected Read Data in read_state " << read_state_;
    // Invoke OnFailed unless it is already invoked.
    if (read_state_ != ERR)
      OnFailed(net::ERR_UNEXPECTED);
    return;
  }
  read_state_ = READING;
  read_buffer_ = read_buffer;

  int bytes_read = bidi_stream_->ReadData(read_buffer_.get(), buffer_size);
  // If IO is pending, wait for the BidirectionalStream to call OnDataRead.
  if (bytes_read == net::ERR_IO_PENDING)
    return;

  if (bytes_read < 0) {
    OnFailed(bytes_read);
    return;
  }
  OnDataRead(bytes_read);
}

void BidirectionalStream::WriteDataOnNetworkThread(
    scoped_refptr<net::WrappedIOBuffer> write_buffer,
    int buffer_size,
    bool end_of_stream) {
  DCHECK(IsOnNetworkThread());
  DCHECK(write_buffer);
  DCHECK(!write_end_of_stream_);
  if (!bidi_stream_ || write_end_of_stream_) {
    DLOG(ERROR) << "Unexpected Flush Data in write_state " << write_state_;
    // Invoke OnFailed unless it is already invoked.
    if (write_state_ != ERR)
      OnFailed(net::ERR_UNEXPECTED);
    return;
  }
  pending_write_data_->AppendBuffer(write_buffer, buffer_size);
  write_end_of_stream_ = end_of_stream;
  if (!disable_auto_flush_)
    FlushOnNetworkThread();
}

void BidirectionalStream::FlushOnNetworkThread() {
  DCHECK(IsOnNetworkThread());
  if (!bidi_stream_)
    return;
  // If there is no data to flush, may need to send headers.
  if (pending_write_data_->Empty()) {
    if (!request_headers_sent_) {
      request_headers_sent_ = true;
      bidi_stream_->SendRequestHeaders();
    }
    return;
  }
  // If request headers are not sent yet, they will be sent with the data.
  if (!request_headers_sent_)
    request_headers_sent_ = true;

  // Move pending data to the flushing list.
  pending_write_data_->MoveTo(flushing_write_data_.get());
  DCHECK(pending_write_data_->Empty());
  if (write_state_ != WRITING)
    SendFlushingWriteData();
}

void BidirectionalStream::SendFlushingWriteData() {
  DCHECK(bidi_stream_);
  // If previous send is not done, or there is nothing to flush, then exit.
  if (write_state_ == WRITING || flushing_write_data_->Empty())
    return;
  DCHECK(sending_write_data_->Empty());
  write_state_ = WRITING;
  flushing_write_data_->MoveTo(sending_write_data_.get());
  bidi_stream_->SendvData(sending_write_data_->buffers(),
                          sending_write_data_->lengths(),
                          write_end_of_stream_ && pending_write_data_->Empty());
}

void BidirectionalStream::CancelOnNetworkThread() {
  DCHECK(IsOnNetworkThread());
  if (!bidi_stream_)
    return;
  read_state_ = write_state_ = CANCELED;
  bidi_stream_.reset();
  weak_factory_.InvalidateWeakPtrs();
  delegate_->OnCanceled();
}

void BidirectionalStream::DestroyOnNetworkThread() {
  DCHECK(IsOnNetworkThread());
  delete this;
}

void BidirectionalStream::MaybeOnSucceded() {
  DCHECK(IsOnNetworkThread());
  if (!bidi_stream_)
    return;
  if (read_state_ == READING_DONE && write_state_ == WRITING_DONE) {
    read_state_ = write_state_ = SUCCESS;
    weak_factory_.InvalidateWeakPtrs();
    // Delete underlying |bidi_stream_| asynchronously as it may still be used.
    PostToNetworkThread(
        FROM_HERE,
        base::BindOnce(&base::DeletePointer<net::BidirectionalStream>,
                       bidi_stream_.release()));
    delegate_->OnSucceeded();
  }
}

bool BidirectionalStream::IsOnNetworkThread() {
  return request_context_getter_->GetNetworkTaskRunner()
      ->BelongsToCurrentThread();
}

void BidirectionalStream::PostToNetworkThread(const base::Location& from_here,
                                              base::OnceClosure task) {
  request_context_getter_->GetNetworkTaskRunner()->PostTask(from_here,
                                                            std::move(task));
}

}  // namespace grpc_support
