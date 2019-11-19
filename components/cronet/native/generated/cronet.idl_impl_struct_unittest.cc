// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* DO NOT EDIT. Generated from components/cronet/native/generated/cronet.idl */

#include "components/cronet/native/generated/cronet.idl_c.h"

#include <math.h>

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

class CronetStructTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}

  CronetStructTest() {}
  ~CronetStructTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CronetStructTest);
};

// Test Struct Cronet_Error setters and getters.
TEST_F(CronetStructTest, TestCronet_Error) {
  Cronet_ErrorPtr first = Cronet_Error_Create();
  Cronet_ErrorPtr second = Cronet_Error_Create();

  // Copy values from |first| to |second|.
  Cronet_Error_error_code_set(second, Cronet_Error_error_code_get(first));
  EXPECT_EQ(Cronet_Error_error_code_get(first),
            Cronet_Error_error_code_get(second));
  Cronet_Error_message_set(second, Cronet_Error_message_get(first));
  EXPECT_STREQ(Cronet_Error_message_get(first),
               Cronet_Error_message_get(second));
  Cronet_Error_internal_error_code_set(
      second, Cronet_Error_internal_error_code_get(first));
  EXPECT_EQ(Cronet_Error_internal_error_code_get(first),
            Cronet_Error_internal_error_code_get(second));
  Cronet_Error_immediately_retryable_set(
      second, Cronet_Error_immediately_retryable_get(first));
  EXPECT_EQ(Cronet_Error_immediately_retryable_get(first),
            Cronet_Error_immediately_retryable_get(second));
  Cronet_Error_quic_detailed_error_code_set(
      second, Cronet_Error_quic_detailed_error_code_get(first));
  EXPECT_EQ(Cronet_Error_quic_detailed_error_code_get(first),
            Cronet_Error_quic_detailed_error_code_get(second));
  Cronet_Error_Destroy(first);
  Cronet_Error_Destroy(second);
}

// Test Struct Cronet_QuicHint setters and getters.
TEST_F(CronetStructTest, TestCronet_QuicHint) {
  Cronet_QuicHintPtr first = Cronet_QuicHint_Create();
  Cronet_QuicHintPtr second = Cronet_QuicHint_Create();

  // Copy values from |first| to |second|.
  Cronet_QuicHint_host_set(second, Cronet_QuicHint_host_get(first));
  EXPECT_STREQ(Cronet_QuicHint_host_get(first),
               Cronet_QuicHint_host_get(second));
  Cronet_QuicHint_port_set(second, Cronet_QuicHint_port_get(first));
  EXPECT_EQ(Cronet_QuicHint_port_get(first), Cronet_QuicHint_port_get(second));
  Cronet_QuicHint_alternate_port_set(second,
                                     Cronet_QuicHint_alternate_port_get(first));
  EXPECT_EQ(Cronet_QuicHint_alternate_port_get(first),
            Cronet_QuicHint_alternate_port_get(second));
  Cronet_QuicHint_Destroy(first);
  Cronet_QuicHint_Destroy(second);
}

// Test Struct Cronet_PublicKeyPins setters and getters.
TEST_F(CronetStructTest, TestCronet_PublicKeyPins) {
  Cronet_PublicKeyPinsPtr first = Cronet_PublicKeyPins_Create();
  Cronet_PublicKeyPinsPtr second = Cronet_PublicKeyPins_Create();

  // Copy values from |first| to |second|.
  Cronet_PublicKeyPins_host_set(second, Cronet_PublicKeyPins_host_get(first));
  EXPECT_STREQ(Cronet_PublicKeyPins_host_get(first),
               Cronet_PublicKeyPins_host_get(second));
  // TODO(mef): Test array |pins_sha256|.
  Cronet_PublicKeyPins_include_subdomains_set(
      second, Cronet_PublicKeyPins_include_subdomains_get(first));
  EXPECT_EQ(Cronet_PublicKeyPins_include_subdomains_get(first),
            Cronet_PublicKeyPins_include_subdomains_get(second));
  Cronet_PublicKeyPins_expiration_date_set(
      second, Cronet_PublicKeyPins_expiration_date_get(first));
  EXPECT_EQ(Cronet_PublicKeyPins_expiration_date_get(first),
            Cronet_PublicKeyPins_expiration_date_get(second));
  Cronet_PublicKeyPins_Destroy(first);
  Cronet_PublicKeyPins_Destroy(second);
}

// Test Struct Cronet_EngineParams setters and getters.
TEST_F(CronetStructTest, TestCronet_EngineParams) {
  Cronet_EngineParamsPtr first = Cronet_EngineParams_Create();
  Cronet_EngineParamsPtr second = Cronet_EngineParams_Create();

  // Copy values from |first| to |second|.
  Cronet_EngineParams_enable_check_result_set(
      second, Cronet_EngineParams_enable_check_result_get(first));
  EXPECT_EQ(Cronet_EngineParams_enable_check_result_get(first),
            Cronet_EngineParams_enable_check_result_get(second));
  Cronet_EngineParams_user_agent_set(second,
                                     Cronet_EngineParams_user_agent_get(first));
  EXPECT_STREQ(Cronet_EngineParams_user_agent_get(first),
               Cronet_EngineParams_user_agent_get(second));
  Cronet_EngineParams_accept_language_set(
      second, Cronet_EngineParams_accept_language_get(first));
  EXPECT_STREQ(Cronet_EngineParams_accept_language_get(first),
               Cronet_EngineParams_accept_language_get(second));
  Cronet_EngineParams_storage_path_set(
      second, Cronet_EngineParams_storage_path_get(first));
  EXPECT_STREQ(Cronet_EngineParams_storage_path_get(first),
               Cronet_EngineParams_storage_path_get(second));
  Cronet_EngineParams_enable_quic_set(
      second, Cronet_EngineParams_enable_quic_get(first));
  EXPECT_EQ(Cronet_EngineParams_enable_quic_get(first),
            Cronet_EngineParams_enable_quic_get(second));
  Cronet_EngineParams_enable_http2_set(
      second, Cronet_EngineParams_enable_http2_get(first));
  EXPECT_EQ(Cronet_EngineParams_enable_http2_get(first),
            Cronet_EngineParams_enable_http2_get(second));
  Cronet_EngineParams_enable_brotli_set(
      second, Cronet_EngineParams_enable_brotli_get(first));
  EXPECT_EQ(Cronet_EngineParams_enable_brotli_get(first),
            Cronet_EngineParams_enable_brotli_get(second));
  Cronet_EngineParams_http_cache_mode_set(
      second, Cronet_EngineParams_http_cache_mode_get(first));
  EXPECT_EQ(Cronet_EngineParams_http_cache_mode_get(first),
            Cronet_EngineParams_http_cache_mode_get(second));
  Cronet_EngineParams_http_cache_max_size_set(
      second, Cronet_EngineParams_http_cache_max_size_get(first));
  EXPECT_EQ(Cronet_EngineParams_http_cache_max_size_get(first),
            Cronet_EngineParams_http_cache_max_size_get(second));
  // TODO(mef): Test array |quic_hints|.
  // TODO(mef): Test array |public_key_pins|.
  Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_set(
      second,
      Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_get(
          first));
  EXPECT_EQ(
      Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_get(
          first),
      Cronet_EngineParams_enable_public_key_pinning_bypass_for_local_trust_anchors_get(
          second));
  Cronet_EngineParams_network_thread_priority_set(
      second, Cronet_EngineParams_network_thread_priority_get(first));
  EXPECT_TRUE(
      Cronet_EngineParams_network_thread_priority_get(first) ==
          Cronet_EngineParams_network_thread_priority_get(second) ||
      isnan(Cronet_EngineParams_network_thread_priority_get(first)) &&
          isnan(Cronet_EngineParams_network_thread_priority_get(second)));
  Cronet_EngineParams_experimental_options_set(
      second, Cronet_EngineParams_experimental_options_get(first));
  EXPECT_STREQ(Cronet_EngineParams_experimental_options_get(first),
               Cronet_EngineParams_experimental_options_get(second));
  Cronet_EngineParams_Destroy(first);
  Cronet_EngineParams_Destroy(second);
}

// Test Struct Cronet_HttpHeader setters and getters.
TEST_F(CronetStructTest, TestCronet_HttpHeader) {
  Cronet_HttpHeaderPtr first = Cronet_HttpHeader_Create();
  Cronet_HttpHeaderPtr second = Cronet_HttpHeader_Create();

  // Copy values from |first| to |second|.
  Cronet_HttpHeader_name_set(second, Cronet_HttpHeader_name_get(first));
  EXPECT_STREQ(Cronet_HttpHeader_name_get(first),
               Cronet_HttpHeader_name_get(second));
  Cronet_HttpHeader_value_set(second, Cronet_HttpHeader_value_get(first));
  EXPECT_STREQ(Cronet_HttpHeader_value_get(first),
               Cronet_HttpHeader_value_get(second));
  Cronet_HttpHeader_Destroy(first);
  Cronet_HttpHeader_Destroy(second);
}

// Test Struct Cronet_UrlResponseInfo setters and getters.
TEST_F(CronetStructTest, TestCronet_UrlResponseInfo) {
  Cronet_UrlResponseInfoPtr first = Cronet_UrlResponseInfo_Create();
  Cronet_UrlResponseInfoPtr second = Cronet_UrlResponseInfo_Create();

  // Copy values from |first| to |second|.
  Cronet_UrlResponseInfo_url_set(second, Cronet_UrlResponseInfo_url_get(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_url_get(first),
               Cronet_UrlResponseInfo_url_get(second));
  // TODO(mef): Test array |url_chain|.
  Cronet_UrlResponseInfo_http_status_code_set(
      second, Cronet_UrlResponseInfo_http_status_code_get(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_http_status_code_get(first),
            Cronet_UrlResponseInfo_http_status_code_get(second));
  Cronet_UrlResponseInfo_http_status_text_set(
      second, Cronet_UrlResponseInfo_http_status_text_get(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_http_status_text_get(first),
               Cronet_UrlResponseInfo_http_status_text_get(second));
  // TODO(mef): Test array |all_headers_list|.
  Cronet_UrlResponseInfo_was_cached_set(
      second, Cronet_UrlResponseInfo_was_cached_get(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_was_cached_get(first),
            Cronet_UrlResponseInfo_was_cached_get(second));
  Cronet_UrlResponseInfo_negotiated_protocol_set(
      second, Cronet_UrlResponseInfo_negotiated_protocol_get(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_negotiated_protocol_get(first),
               Cronet_UrlResponseInfo_negotiated_protocol_get(second));
  Cronet_UrlResponseInfo_proxy_server_set(
      second, Cronet_UrlResponseInfo_proxy_server_get(first));
  EXPECT_STREQ(Cronet_UrlResponseInfo_proxy_server_get(first),
               Cronet_UrlResponseInfo_proxy_server_get(second));
  Cronet_UrlResponseInfo_received_byte_count_set(
      second, Cronet_UrlResponseInfo_received_byte_count_get(first));
  EXPECT_EQ(Cronet_UrlResponseInfo_received_byte_count_get(first),
            Cronet_UrlResponseInfo_received_byte_count_get(second));
  Cronet_UrlResponseInfo_Destroy(first);
  Cronet_UrlResponseInfo_Destroy(second);
}

// Test Struct Cronet_UrlRequestParams setters and getters.
TEST_F(CronetStructTest, TestCronet_UrlRequestParams) {
  Cronet_UrlRequestParamsPtr first = Cronet_UrlRequestParams_Create();
  Cronet_UrlRequestParamsPtr second = Cronet_UrlRequestParams_Create();

  // Copy values from |first| to |second|.
  Cronet_UrlRequestParams_http_method_set(
      second, Cronet_UrlRequestParams_http_method_get(first));
  EXPECT_STREQ(Cronet_UrlRequestParams_http_method_get(first),
               Cronet_UrlRequestParams_http_method_get(second));
  // TODO(mef): Test array |request_headers|.
  Cronet_UrlRequestParams_disable_cache_set(
      second, Cronet_UrlRequestParams_disable_cache_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_disable_cache_get(first),
            Cronet_UrlRequestParams_disable_cache_get(second));
  Cronet_UrlRequestParams_priority_set(
      second, Cronet_UrlRequestParams_priority_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_priority_get(first),
            Cronet_UrlRequestParams_priority_get(second));
  Cronet_UrlRequestParams_upload_data_provider_set(
      second, Cronet_UrlRequestParams_upload_data_provider_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_upload_data_provider_get(first),
            Cronet_UrlRequestParams_upload_data_provider_get(second));
  Cronet_UrlRequestParams_upload_data_provider_executor_set(
      second, Cronet_UrlRequestParams_upload_data_provider_executor_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_upload_data_provider_executor_get(first),
            Cronet_UrlRequestParams_upload_data_provider_executor_get(second));
  Cronet_UrlRequestParams_allow_direct_executor_set(
      second, Cronet_UrlRequestParams_allow_direct_executor_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_allow_direct_executor_get(first),
            Cronet_UrlRequestParams_allow_direct_executor_get(second));
  // TODO(mef): Test array |annotations|.
  Cronet_UrlRequestParams_request_finished_listener_set(
      second, Cronet_UrlRequestParams_request_finished_listener_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_request_finished_listener_get(first),
            Cronet_UrlRequestParams_request_finished_listener_get(second));
  Cronet_UrlRequestParams_request_finished_executor_set(
      second, Cronet_UrlRequestParams_request_finished_executor_get(first));
  EXPECT_EQ(Cronet_UrlRequestParams_request_finished_executor_get(first),
            Cronet_UrlRequestParams_request_finished_executor_get(second));
  Cronet_UrlRequestParams_Destroy(first);
  Cronet_UrlRequestParams_Destroy(second);
}

// Test Struct Cronet_DateTime setters and getters.
TEST_F(CronetStructTest, TestCronet_DateTime) {
  Cronet_DateTimePtr first = Cronet_DateTime_Create();
  Cronet_DateTimePtr second = Cronet_DateTime_Create();

  // Copy values from |first| to |second|.
  Cronet_DateTime_value_set(second, Cronet_DateTime_value_get(first));
  EXPECT_EQ(Cronet_DateTime_value_get(first),
            Cronet_DateTime_value_get(second));
  Cronet_DateTime_Destroy(first);
  Cronet_DateTime_Destroy(second);
}

// Test Struct Cronet_Metrics setters and getters.
TEST_F(CronetStructTest, TestCronet_Metrics) {
  Cronet_MetricsPtr first = Cronet_Metrics_Create();
  Cronet_MetricsPtr second = Cronet_Metrics_Create();

  // Copy values from |first| to |second|.
  Cronet_DateTimePtr test_request_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_request_start_get(first), nullptr);

  Cronet_Metrics_request_start_set(first, test_request_start);
  EXPECT_NE(Cronet_Metrics_request_start_get(first), nullptr);
  Cronet_Metrics_request_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_request_start_get(first), nullptr);

  Cronet_Metrics_request_start_move(first, test_request_start);
  EXPECT_NE(Cronet_Metrics_request_start_get(first), nullptr);
  Cronet_Metrics_request_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_request_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_request_start);
  Cronet_DateTimePtr test_dns_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_dns_start_get(first), nullptr);

  Cronet_Metrics_dns_start_set(first, test_dns_start);
  EXPECT_NE(Cronet_Metrics_dns_start_get(first), nullptr);
  Cronet_Metrics_dns_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_dns_start_get(first), nullptr);

  Cronet_Metrics_dns_start_move(first, test_dns_start);
  EXPECT_NE(Cronet_Metrics_dns_start_get(first), nullptr);
  Cronet_Metrics_dns_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_dns_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_dns_start);
  Cronet_DateTimePtr test_dns_end = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_dns_end_get(first), nullptr);

  Cronet_Metrics_dns_end_set(first, test_dns_end);
  EXPECT_NE(Cronet_Metrics_dns_end_get(first), nullptr);
  Cronet_Metrics_dns_end_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_dns_end_get(first), nullptr);

  Cronet_Metrics_dns_end_move(first, test_dns_end);
  EXPECT_NE(Cronet_Metrics_dns_end_get(first), nullptr);
  Cronet_Metrics_dns_end_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_dns_end_get(first), nullptr);

  Cronet_DateTime_Destroy(test_dns_end);
  Cronet_DateTimePtr test_connect_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_connect_start_get(first), nullptr);

  Cronet_Metrics_connect_start_set(first, test_connect_start);
  EXPECT_NE(Cronet_Metrics_connect_start_get(first), nullptr);
  Cronet_Metrics_connect_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_connect_start_get(first), nullptr);

  Cronet_Metrics_connect_start_move(first, test_connect_start);
  EXPECT_NE(Cronet_Metrics_connect_start_get(first), nullptr);
  Cronet_Metrics_connect_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_connect_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_connect_start);
  Cronet_DateTimePtr test_connect_end = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_connect_end_get(first), nullptr);

  Cronet_Metrics_connect_end_set(first, test_connect_end);
  EXPECT_NE(Cronet_Metrics_connect_end_get(first), nullptr);
  Cronet_Metrics_connect_end_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_connect_end_get(first), nullptr);

  Cronet_Metrics_connect_end_move(first, test_connect_end);
  EXPECT_NE(Cronet_Metrics_connect_end_get(first), nullptr);
  Cronet_Metrics_connect_end_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_connect_end_get(first), nullptr);

  Cronet_DateTime_Destroy(test_connect_end);
  Cronet_DateTimePtr test_ssl_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_ssl_start_get(first), nullptr);

  Cronet_Metrics_ssl_start_set(first, test_ssl_start);
  EXPECT_NE(Cronet_Metrics_ssl_start_get(first), nullptr);
  Cronet_Metrics_ssl_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_ssl_start_get(first), nullptr);

  Cronet_Metrics_ssl_start_move(first, test_ssl_start);
  EXPECT_NE(Cronet_Metrics_ssl_start_get(first), nullptr);
  Cronet_Metrics_ssl_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_ssl_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_ssl_start);
  Cronet_DateTimePtr test_ssl_end = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_ssl_end_get(first), nullptr);

  Cronet_Metrics_ssl_end_set(first, test_ssl_end);
  EXPECT_NE(Cronet_Metrics_ssl_end_get(first), nullptr);
  Cronet_Metrics_ssl_end_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_ssl_end_get(first), nullptr);

  Cronet_Metrics_ssl_end_move(first, test_ssl_end);
  EXPECT_NE(Cronet_Metrics_ssl_end_get(first), nullptr);
  Cronet_Metrics_ssl_end_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_ssl_end_get(first), nullptr);

  Cronet_DateTime_Destroy(test_ssl_end);
  Cronet_DateTimePtr test_sending_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_sending_start_get(first), nullptr);

  Cronet_Metrics_sending_start_set(first, test_sending_start);
  EXPECT_NE(Cronet_Metrics_sending_start_get(first), nullptr);
  Cronet_Metrics_sending_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_sending_start_get(first), nullptr);

  Cronet_Metrics_sending_start_move(first, test_sending_start);
  EXPECT_NE(Cronet_Metrics_sending_start_get(first), nullptr);
  Cronet_Metrics_sending_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_sending_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_sending_start);
  Cronet_DateTimePtr test_sending_end = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_sending_end_get(first), nullptr);

  Cronet_Metrics_sending_end_set(first, test_sending_end);
  EXPECT_NE(Cronet_Metrics_sending_end_get(first), nullptr);
  Cronet_Metrics_sending_end_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_sending_end_get(first), nullptr);

  Cronet_Metrics_sending_end_move(first, test_sending_end);
  EXPECT_NE(Cronet_Metrics_sending_end_get(first), nullptr);
  Cronet_Metrics_sending_end_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_sending_end_get(first), nullptr);

  Cronet_DateTime_Destroy(test_sending_end);
  Cronet_DateTimePtr test_push_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_push_start_get(first), nullptr);

  Cronet_Metrics_push_start_set(first, test_push_start);
  EXPECT_NE(Cronet_Metrics_push_start_get(first), nullptr);
  Cronet_Metrics_push_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_push_start_get(first), nullptr);

  Cronet_Metrics_push_start_move(first, test_push_start);
  EXPECT_NE(Cronet_Metrics_push_start_get(first), nullptr);
  Cronet_Metrics_push_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_push_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_push_start);
  Cronet_DateTimePtr test_push_end = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_push_end_get(first), nullptr);

  Cronet_Metrics_push_end_set(first, test_push_end);
  EXPECT_NE(Cronet_Metrics_push_end_get(first), nullptr);
  Cronet_Metrics_push_end_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_push_end_get(first), nullptr);

  Cronet_Metrics_push_end_move(first, test_push_end);
  EXPECT_NE(Cronet_Metrics_push_end_get(first), nullptr);
  Cronet_Metrics_push_end_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_push_end_get(first), nullptr);

  Cronet_DateTime_Destroy(test_push_end);
  Cronet_DateTimePtr test_response_start = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_response_start_get(first), nullptr);

  Cronet_Metrics_response_start_set(first, test_response_start);
  EXPECT_NE(Cronet_Metrics_response_start_get(first), nullptr);
  Cronet_Metrics_response_start_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_response_start_get(first), nullptr);

  Cronet_Metrics_response_start_move(first, test_response_start);
  EXPECT_NE(Cronet_Metrics_response_start_get(first), nullptr);
  Cronet_Metrics_response_start_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_response_start_get(first), nullptr);

  Cronet_DateTime_Destroy(test_response_start);
  Cronet_DateTimePtr test_request_end = Cronet_DateTime_Create();
  EXPECT_EQ(Cronet_Metrics_request_end_get(first), nullptr);

  Cronet_Metrics_request_end_set(first, test_request_end);
  EXPECT_NE(Cronet_Metrics_request_end_get(first), nullptr);
  Cronet_Metrics_request_end_set(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_request_end_get(first), nullptr);

  Cronet_Metrics_request_end_move(first, test_request_end);
  EXPECT_NE(Cronet_Metrics_request_end_get(first), nullptr);
  Cronet_Metrics_request_end_move(first, nullptr);
  EXPECT_EQ(Cronet_Metrics_request_end_get(first), nullptr);

  Cronet_DateTime_Destroy(test_request_end);
  Cronet_Metrics_socket_reused_set(second,
                                   Cronet_Metrics_socket_reused_get(first));
  EXPECT_EQ(Cronet_Metrics_socket_reused_get(first),
            Cronet_Metrics_socket_reused_get(second));
  Cronet_Metrics_sent_byte_count_set(second,
                                     Cronet_Metrics_sent_byte_count_get(first));
  EXPECT_EQ(Cronet_Metrics_sent_byte_count_get(first),
            Cronet_Metrics_sent_byte_count_get(second));
  Cronet_Metrics_received_byte_count_set(
      second, Cronet_Metrics_received_byte_count_get(first));
  EXPECT_EQ(Cronet_Metrics_received_byte_count_get(first),
            Cronet_Metrics_received_byte_count_get(second));
  Cronet_Metrics_Destroy(first);
  Cronet_Metrics_Destroy(second);
}

// Test Struct Cronet_RequestFinishedInfo setters and getters.
TEST_F(CronetStructTest, TestCronet_RequestFinishedInfo) {
  Cronet_RequestFinishedInfoPtr first = Cronet_RequestFinishedInfo_Create();
  Cronet_RequestFinishedInfoPtr second = Cronet_RequestFinishedInfo_Create();

  // Copy values from |first| to |second|.
  Cronet_MetricsPtr test_metrics = Cronet_Metrics_Create();
  EXPECT_EQ(Cronet_RequestFinishedInfo_metrics_get(first), nullptr);

  Cronet_RequestFinishedInfo_metrics_set(first, test_metrics);
  EXPECT_NE(Cronet_RequestFinishedInfo_metrics_get(first), nullptr);
  Cronet_RequestFinishedInfo_metrics_set(first, nullptr);
  EXPECT_EQ(Cronet_RequestFinishedInfo_metrics_get(first), nullptr);

  Cronet_RequestFinishedInfo_metrics_move(first, test_metrics);
  EXPECT_NE(Cronet_RequestFinishedInfo_metrics_get(first), nullptr);
  Cronet_RequestFinishedInfo_metrics_move(first, nullptr);
  EXPECT_EQ(Cronet_RequestFinishedInfo_metrics_get(first), nullptr);

  Cronet_Metrics_Destroy(test_metrics);
  // TODO(mef): Test array |annotations|.
  Cronet_RequestFinishedInfo_finished_reason_set(
      second, Cronet_RequestFinishedInfo_finished_reason_get(first));
  EXPECT_EQ(Cronet_RequestFinishedInfo_finished_reason_get(first),
            Cronet_RequestFinishedInfo_finished_reason_get(second));
  Cronet_RequestFinishedInfo_Destroy(first);
  Cronet_RequestFinishedInfo_Destroy(second);
}
