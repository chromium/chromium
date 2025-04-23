// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/web_resource_response.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/native_j_unittests_jni_headers/WebResourceResponseUnittest_jni.h"

namespace embedder_support {
class WebResourceResponseTest : public testing::Test {
 public:
  WebResourceResponseTest() = default;

 protected:
  void SetUp() override {
    env_ = jni_zero::AttachCurrentThread();
    ASSERT_THAT(env_, testing::NotNull());
  }

  std::multimap<std::string, std::string> HeadersToMap(
      const net::HttpResponseHeaders* headers) {
    std::multimap<std::string, std::string> result;
    size_t iter = 0;
    std::string name;
    std::string value;
    while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
      result.insert({name, value});
    }
    return result;
  }

  raw_ptr<JNIEnv> env_;
};

TEST_F(WebResourceResponseTest, TestNullJniConversion) {
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_getNullResponse(env_);
  EXPECT_FALSE(response);
}

TEST_F(WebResourceResponseTest, CanReadAllFields) {
  std::string mime_type = "text/plain";
  std::string charset = "utf-8";
  int status_code = 217;
  std::string reason_phrase = "Reason Overridden";
  base::flat_map<std::string, std::string> headers{
      {"X-Header", "value1"},
      {"Y-Header", "value2"},
  };
  std::string body = "Body Content";

  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, mime_type, charset, status_code, reason_phrase, headers, body);
  EXPECT_TRUE(response);

  std::string received_mime_type;
  EXPECT_TRUE(response->GetMimeType(env_, &received_mime_type));
  EXPECT_EQ(mime_type, received_mime_type);

  std::string received_charset;
  EXPECT_TRUE(response->GetCharset(env_, &received_charset));
  EXPECT_EQ(charset, received_charset);

  int received_status_code;
  std::string received_reason_phrase;
  EXPECT_TRUE(response->GetStatusInfo(env_, &received_status_code,
                                      &received_reason_phrase));
  EXPECT_EQ(status_code, received_status_code);
  EXPECT_EQ(reason_phrase, received_reason_phrase);

  scoped_refptr<net::HttpResponseHeaders> received_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  EXPECT_TRUE(response->GetResponseHeaders(env_, received_headers.get()));
  std::multimap<std::string, std::string> received_header_map =
      HeadersToMap(received_headers.get());
  EXPECT_EQ(2UL, received_header_map.size());
  EXPECT_EQ(1UL, received_header_map.count("X-Header"));
  EXPECT_EQ("value1", received_header_map.find("X-Header")->second);
  EXPECT_EQ(1UL, received_header_map.count("Y-Header"));
  EXPECT_EQ("value2", received_header_map.find("Y-Header")->second);

  EXPECT_TRUE(response->HasInputStream(env_));
  std::unique_ptr<InputStream> input_stream = response->GetInputStream(env_);
  EXPECT_TRUE(input_stream);

  int received_bytes_available;
  input_stream->BytesAvailable(&received_bytes_available);
  EXPECT_EQ(body.length(), (size_t)received_bytes_available);
}

TEST_F(WebResourceResponseTest, HandlesNullHeaderMap) {
  // Construct a response with a null header map.
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, "text/plain", "utf-8", 200, "OK", "body content");

  scoped_refptr<net::HttpResponseHeaders> received_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  EXPECT_FALSE(response->GetResponseHeaders(env_, received_headers.get()));
  std::multimap<std::string, std::string> received_header_map =
      HeadersToMap(received_headers.get());
  EXPECT_EQ(0UL, received_header_map.size())
      << "No headers should be added to the HttpResponseHeaders";
}

TEST_F(WebResourceResponseTest, IgnoresLowStatusCodes) {
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, "text/plain", "utf-8", 50, "OK", "body content");

  int received_status_code = -1;
  std::string received_reason_phrase = "unchanged";
  EXPECT_FALSE(response->GetStatusInfo(env_, &received_status_code,
                                       &received_reason_phrase));
  EXPECT_EQ(-1, received_status_code) << "No change to variable value expected";
  EXPECT_EQ("unchanged", received_reason_phrase)
      << "No change to variable value expected";
}

TEST_F(WebResourceResponseTest, IgnoresHighStatusCodes) {
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, "text/plain", "utf-8", 600, "OK", "body content");

  int received_status_code = -1;
  std::string received_reason_phrase = "unchanged";
  EXPECT_FALSE(response->GetStatusInfo(env_, &received_status_code,
                                       &received_reason_phrase));
  EXPECT_EQ(-1, received_status_code) << "No change to variable value expected";
  EXPECT_EQ("unchanged", received_reason_phrase)
      << "No change to variable value expected";
}

TEST_F(WebResourceResponseTest, IgnoresUnsetReasonPhrase) {
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, "text/plain", "utf-8", 200, std::nullopt, "body content");

  int received_status_code = -1;
  std::string received_reason_phrase = "unchanged";
  EXPECT_FALSE(response->GetStatusInfo(env_, &received_status_code,
                                       &received_reason_phrase));
  EXPECT_EQ(-1, received_status_code) << "No change to variable value expected";
  EXPECT_EQ("unchanged", received_reason_phrase)
      << "No change to variable value expected";
}

TEST_F(WebResourceResponseTest, IgnoresUnsetMimeType) {
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, std::nullopt, "utf-8", 200, "OK", "body content");

  std::string received_mime_type = "unchanged";
  EXPECT_FALSE(response->GetMimeType(env_, &received_mime_type));
  EXPECT_EQ("unchanged", received_mime_type)
      << "No change to variable value expected";
}

TEST_F(WebResourceResponseTest, IgnoresUnsetCharset) {
  std::unique_ptr<WebResourceResponse> response =
      Java_WebResourceResponseUnittest_createJavaObject(
          env_, "text/plain", std::nullopt, 200, "OK", "body content");

  std::string received_charset = "unchanged";
  EXPECT_FALSE(response->GetCharset(env_, &received_charset));
  EXPECT_EQ("unchanged", received_charset)
      << "No change to variable value expected";
}

}  // namespace embedder_support
