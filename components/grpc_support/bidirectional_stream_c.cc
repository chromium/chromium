// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/grpc_support/include/bidirectional_stream_c.h"

#include <stdbool.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/grpc_support/bidirectional_stream.h"
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
#include "url/gurl.h"

namespace {

class HeadersArray : public bidirectional_stream_header_array {
 public:
  explicit HeadersArray(const spdy::SpdyHeaderBlock& header_block);
  ~HeadersArray();

 private:
  base::StringPairs headers_strings_;
  DISALLOW_COPY_AND_ASSIGN(HeadersArray);
};

HeadersArray::HeadersArray(const spdy::SpdyHeaderBlock& header_block)
    : headers_strings_(header_block.size()) {
  // Count and headers are inherited from parent structure.
  count = capacity = header_block.size();
  headers = new bidirectional_stream_header[count];
  size_t i = 0;
  // Copy headers into |headers_strings_| because string pieces are not
  // '\0'-terminated.
  for (const auto& it : header_block) {
    headers_strings_[i].first = it.first.as_string();
    headers_strings_[i].second = it.second.as_string();
    headers[i].key = headers_strings_[i].first.c_str();
    headers[i].value = headers_strings_[i].second.c_str();
    ++i;
  }
}

HeadersArray::~HeadersArray() {
  delete[] headers;
}

class BidirectionalStreamAdapter
    : public grpc_support::BidirectionalStream::Delegate {
 public:
  BidirectionalStreamAdapter(stream_engine* engine,
                             void* annotation,
                             bidirectional_stream_callback* callback);

  virtual ~BidirectionalStreamAdapter();

  void OnStreamReady() override;

  void OnHeadersReceived(const spdy::SpdyHeaderBlock& headers_block,
                         const char* negotiated_protocol) override;

  void OnDataRead(char* data, int size) override;

  void OnDataSent(const char* data) override;

  void OnTrailersReceived(const spdy::SpdyHeaderBlock& trailers_block) override;

  void OnSucceeded() override;

  void OnFailed(int error) override;

  void OnCanceled() override;

  bidirectional_stream* c_stream() const { return c_stream_.get(); }

  static grpc_support::BidirectionalStream* GetStream(
      bidirectional_stream* stream);

  static void DestroyAdapterForStream(bidirectional_stream* stream);

 private:
  void DestroyOnNetworkThread();

  // None of these objects are owned by |this|.
  net::URLRequestContextGetter* request_context_getter_;
  grpc_support::BidirectionalStream* bidirectional_stream_;
  // C side
  std::unique_ptr<bidirectional_stream> c_stream_;
  bidirectional_stream_callback* c_callback_;
};

BidirectionalStreamAdapter::BidirectionalStreamAdapter(
    stream_engine* engine,
    void* annotation,
    bidirectional_stream_callback* callback)
    : request_context_getter_(
          reinterpret_cast<net::URLRequestContextGetter*>(engine->obj)),
      c_stream_(std::make_unique<bidirectional_stream>()),
      c_callback_(callback) {
  DCHECK(request_context_getter_);
  bidirectional_stream_ =
      new grpc_support::BidirectionalStream(request_context_getter_, this);
  c_stream_->obj = this;
  c_stream_->annotation = annotation;
}

BidirectionalStreamAdapter::~BidirectionalStreamAdapter() {}

void BidirectionalStreamAdapter::OnStreamReady() {
  DCHECK(c_callback_->on_response_headers_received);
  c_callback_->on_stream_ready(c_stream());
}

void BidirectionalStreamAdapter::OnHeadersReceived(
    const spdy::SpdyHeaderBlock& headers_block,
    const char* negotiated_protocol) {
  DCHECK(c_callback_->on_response_headers_received);
  HeadersArray response_headers(headers_block);
  c_callback_->on_response_headers_received(c_stream(), &response_headers,
                                            negotiated_protocol);
}

void BidirectionalStreamAdapter::OnDataRead(char* data, int size) {
  DCHECK(c_callback_->on_read_completed);
  c_callback_->on_read_completed(c_stream(), data, size);
}

void BidirectionalStreamAdapter::OnDataSent(const char* data) {
  DCHECK(c_callback_->on_write_completed);
  c_callback_->on_write_completed(c_stream(), data);
}

void BidirectionalStreamAdapter::OnTrailersReceived(
    const spdy::SpdyHeaderBlock& trailers_block) {
  DCHECK(c_callback_->on_response_trailers_received);
  HeadersArray response_trailers(trailers_block);
  c_callback_->on_response_trailers_received(c_stream(), &response_trailers);
}

void BidirectionalStreamAdapter::OnSucceeded() {
  DCHECK(c_callback_->on_succeded);
  c_callback_->on_succeded(c_stream());
}

void BidirectionalStreamAdapter::OnFailed(int error) {
  DCHECK(c_callback_->on_failed);
  c_callback_->on_failed(c_stream(), error);
}

void BidirectionalStreamAdapter::OnCanceled() {
  DCHECK(c_callback_->on_canceled);
  c_callback_->on_canceled(c_stream());
}

grpc_support::BidirectionalStream* BidirectionalStreamAdapter::GetStream(
    bidirectional_stream* stream) {
  DCHECK(stream);
  BidirectionalStreamAdapter* adapter =
      static_cast<BidirectionalStreamAdapter*>(stream->obj);
  DCHECK(adapter->c_stream() == stream);
  DCHECK(adapter->bidirectional_stream_);
  return adapter->bidirectional_stream_;
}

void BidirectionalStreamAdapter::DestroyAdapterForStream(
    bidirectional_stream* stream) {
  DCHECK(stream);
  BidirectionalStreamAdapter* adapter =
      static_cast<BidirectionalStreamAdapter*>(stream->obj);
  DCHECK(adapter->c_stream() == stream);
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete.
  adapter->bidirectional_stream_->Destroy();
  adapter->request_context_getter_->GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BidirectionalStreamAdapter::DestroyOnNetworkThread,
                     base::Unretained(adapter)));
}

void BidirectionalStreamAdapter::DestroyOnNetworkThread() {
  DCHECK(request_context_getter_->GetNetworkTaskRunner()
             ->BelongsToCurrentThread());
  delete this;
}

}  // namespace

bidirectional_stream* bidirectional_stream_create(
    stream_engine* engine,
    void* annotation,
    bidirectional_stream_callback* callback) {
  // Allocate new C++ adapter that will invoke |callback|.
  BidirectionalStreamAdapter* stream_adapter =
      new BidirectionalStreamAdapter(engine, annotation, callback);
  return stream_adapter->c_stream();
}

int bidirectional_stream_destroy(bidirectional_stream* stream) {
  BidirectionalStreamAdapter::DestroyAdapterForStream(stream);
  return 1;
}

void bidirectional_stream_disable_auto_flush(bidirectional_stream* stream,
                                             bool disable_auto_flush) {
  BidirectionalStreamAdapter::GetStream(stream)->disable_auto_flush(
      disable_auto_flush);
}

void bidirectional_stream_delay_request_headers_until_flush(
    bidirectional_stream* stream,
    bool delay_headers_until_flush) {
  BidirectionalStreamAdapter::GetStream(stream)->delay_headers_until_flush(
      delay_headers_until_flush);
}

int bidirectional_stream_start(bidirectional_stream* stream,
                               const char* url,
                               int priority,
                               const char* method,
                               const bidirectional_stream_header_array* headers,
                               bool end_of_stream) {
  grpc_support::BidirectionalStream* internal_stream =
      BidirectionalStreamAdapter::GetStream(stream);
  net::HttpRequestHeaders request_headers;
  if (headers) {
    for (size_t i = 0; i < headers->count; ++i) {
      std::string name(headers->headers[i].key);
      std::string value(headers->headers[i].value);
      if (!net::HttpUtil::IsValidHeaderName(name) ||
          !net::HttpUtil::IsValidHeaderValue(value)) {
        DLOG(ERROR) << "Invalid Header " << name << "=" << value;
        return i + 1;
      }
      request_headers.SetHeader(name, value);
    }
  }
  return internal_stream->Start(url, priority, method, request_headers,
                                end_of_stream);
}

int bidirectional_stream_read(bidirectional_stream* stream,
                              char* buffer,
                              int capacity) {
  return BidirectionalStreamAdapter::GetStream(stream)->ReadData(buffer,
                                                                 capacity);
}

int bidirectional_stream_write(bidirectional_stream* stream,
                               const char* buffer,
                               int count,
                               bool end_of_stream) {
  return BidirectionalStreamAdapter::GetStream(stream)->WriteData(
      buffer, count, end_of_stream);
}

void bidirectional_stream_flush(bidirectional_stream* stream) {
  return BidirectionalStreamAdapter::GetStream(stream)->Flush();
}

void bidirectional_stream_cancel(bidirectional_stream* stream) {
  BidirectionalStreamAdapter::GetStream(stream)->Cancel();
}
