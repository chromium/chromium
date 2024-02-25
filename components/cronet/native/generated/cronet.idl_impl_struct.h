// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#ifndef COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_STRUCT_H_
#define COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_STRUCT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/cronet/native/generated/cronet.idl_c.h"

// Struct Cronet_Error.
struct Cronet_Error {
 public:
  Cronet_Error();
  explicit Cronet_Error(const Cronet_Error& from);

  Cronet_Error& operator=(const Cronet_Error&) = delete;

  explicit Cronet_Error(Cronet_Error&& from);

  ~Cronet_Error();

  Cronet_Error_ERROR_CODE error_code = Cronet_Error_ERROR_CODE_ERROR_CALLBACK;
  std::string message;
  int32_t internal_error_code = 0;
  bool immediately_retryable = false;
  int32_t quic_detailed_error_code = 0;
};

// Struct Cronet_QuicHint.
struct Cronet_QuicHint {
 public:
  Cronet_QuicHint();
  explicit Cronet_QuicHint(const Cronet_QuicHint& from);

  Cronet_QuicHint& operator=(const Cronet_QuicHint&) = delete;

  explicit Cronet_QuicHint(Cronet_QuicHint&& from);

  ~Cronet_QuicHint();

  std::string host;
  int32_t port = 0;
  int32_t alternate_port = 0;
};

// Struct Cronet_PublicKeyPins.
struct Cronet_PublicKeyPins {
 public:
  Cronet_PublicKeyPins();
  explicit Cronet_PublicKeyPins(const Cronet_PublicKeyPins& from);

  Cronet_PublicKeyPins& operator=(const Cronet_PublicKeyPins&) = delete;

  explicit Cronet_PublicKeyPins(Cronet_PublicKeyPins&& from);

  ~Cronet_PublicKeyPins();

  std::string host;
  std::vector<std::string> pins_sha256;
  bool include_subdomains = false;
  int64_t expiration_date = 0;
};

// Struct Cronet_EngineParams.
struct Cronet_EngineParams {
 public:
  Cronet_EngineParams();
  explicit Cronet_EngineParams(const Cronet_EngineParams& from);

  Cronet_EngineParams& operator=(const Cronet_EngineParams&) = delete;

  explicit Cronet_EngineParams(Cronet_EngineParams&& from);

  ~Cronet_EngineParams();

  bool enable_check_result = true;
  std::string user_agent;
  std::string accept_language;
  std::string storage_path;
  bool enable_quic = true;
  bool enable_http2 = true;
  bool enable_brotli = true;
  Cronet_EngineParams_HTTP_CACHE_MODE http_cache_mode =
      Cronet_EngineParams_HTTP_CACHE_MODE_DISABLED;
  int64_t http_cache_max_size = 0;
  std::vector<Cronet_QuicHint> quic_hints;
  std::vector<Cronet_PublicKeyPins> public_key_pins;
  bool enable_public_key_pinning_bypass_for_local_trust_anchors = true;
  double network_thread_priority = std::numeric_limits<double>::quiet_NaN();
  std::string experimental_options;
};

// Struct Cronet_HttpHeader.
struct Cronet_HttpHeader {
 public:
  Cronet_HttpHeader();
  explicit Cronet_HttpHeader(const Cronet_HttpHeader& from);

  Cronet_HttpHeader& operator=(const Cronet_HttpHeader&) = delete;

  explicit Cronet_HttpHeader(Cronet_HttpHeader&& from);

  ~Cronet_HttpHeader();

  std::string name;
  std::string value;
};

// Struct Cronet_UrlResponseInfo.
struct Cronet_UrlResponseInfo {
 public:
  Cronet_UrlResponseInfo();
  explicit Cronet_UrlResponseInfo(const Cronet_UrlResponseInfo& from);

  Cronet_UrlResponseInfo& operator=(const Cronet_UrlResponseInfo&) = delete;

  explicit Cronet_UrlResponseInfo(Cronet_UrlResponseInfo&& from);

  ~Cronet_UrlResponseInfo();

  std::string url;
  std::vector<std::string> url_chain;
  int32_t http_status_code = 0;
  std::string http_status_text;
  std::vector<Cronet_HttpHeader> all_headers_list;
  bool was_cached = false;
  std::string negotiated_protocol;
  std::string proxy_server;
  int64_t received_byte_count = 0;
};

// Struct Cronet_UrlRequestParams.
struct Cronet_UrlRequestParams {
 public:
  Cronet_UrlRequestParams();
  explicit Cronet_UrlRequestParams(const Cronet_UrlRequestParams& from);

  Cronet_UrlRequestParams& operator=(const Cronet_UrlRequestParams&) = delete;

  explicit Cronet_UrlRequestParams(Cronet_UrlRequestParams&& from);

  ~Cronet_UrlRequestParams();

  std::string http_method;
  std::vector<Cronet_HttpHeader> request_headers;
  bool disable_cache = false;
  Cronet_UrlRequestParams_REQUEST_PRIORITY priority =
      Cronet_UrlRequestParams_REQUEST_PRIORITY_REQUEST_PRIORITY_MEDIUM;
  Cronet_UploadDataProviderPtr upload_data_provider = nullptr;
  Cronet_ExecutorPtr upload_data_provider_executor = nullptr;
  bool allow_direct_executor = false;
  std::vector<Cronet_RawDataPtr> annotations;
  Cronet_RequestFinishedInfoListenerPtr request_finished_listener = nullptr;
  Cronet_ExecutorPtr request_finished_executor = nullptr;
  Cronet_UrlRequestParams_IDEMPOTENCY idempotency =
      Cronet_UrlRequestParams_IDEMPOTENCY_DEFAULT_IDEMPOTENCY;
};

// Struct Cronet_DateTime.
struct Cronet_DateTime {
 public:
  Cronet_DateTime();
  explicit Cronet_DateTime(const Cronet_DateTime& from);

  Cronet_DateTime& operator=(const Cronet_DateTime&) = delete;

  explicit Cronet_DateTime(Cronet_DateTime&& from);

  ~Cronet_DateTime();

  int64_t value = 0;
};

// Struct Cronet_Metrics.
struct Cronet_Metrics {
 public:
  Cronet_Metrics();
  explicit Cronet_Metrics(const Cronet_Metrics& from);

  Cronet_Metrics& operator=(const Cronet_Metrics&) = delete;

  explicit Cronet_Metrics(Cronet_Metrics&& from);

  ~Cronet_Metrics();

  std::optional<Cronet_DateTime> request_start;
  std::optional<Cronet_DateTime> dns_start;
  std::optional<Cronet_DateTime> dns_end;
  std::optional<Cronet_DateTime> connect_start;
  std::optional<Cronet_DateTime> connect_end;
  std::optional<Cronet_DateTime> ssl_start;
  std::optional<Cronet_DateTime> ssl_end;
  std::optional<Cronet_DateTime> sending_start;
  std::optional<Cronet_DateTime> sending_end;
  std::optional<Cronet_DateTime> push_start;
  std::optional<Cronet_DateTime> push_end;
  std::optional<Cronet_DateTime> response_start;
  std::optional<Cronet_DateTime> request_end;
  bool socket_reused = false;
  int64_t sent_byte_count = -1;
  int64_t received_byte_count = -1;
};

// Struct Cronet_RequestFinishedInfo.
struct Cronet_RequestFinishedInfo {
 public:
  Cronet_RequestFinishedInfo();
  explicit Cronet_RequestFinishedInfo(const Cronet_RequestFinishedInfo& from);

  Cronet_RequestFinishedInfo& operator=(const Cronet_RequestFinishedInfo&) =
      delete;

  explicit Cronet_RequestFinishedInfo(Cronet_RequestFinishedInfo&& from);

  ~Cronet_RequestFinishedInfo();

  std::optional<Cronet_Metrics> metrics;
  std::vector<Cronet_RawDataPtr> annotations;
  Cronet_RequestFinishedInfo_FINISHED_REASON finished_reason =
      Cronet_RequestFinishedInfo_FINISHED_REASON_SUCCEEDED;
};

#endif  // COMPONENTS_CRONET_NATIVE_GENERATED_CRONET_IDL_IMPL_STRUCT_H_
