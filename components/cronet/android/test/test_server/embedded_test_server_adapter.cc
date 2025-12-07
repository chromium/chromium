// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/cronet/android/test/test_server/embedded_test_server_adapter.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/android/jni_string.h"
#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_support_android.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace cronet {

using NativeTestServerHeaderMap =
    std::map<std::string,
             std::string,
             net::test_server::HttpRequest::CaseInsensitiveStringComparator>;

struct NativeTestServerHttpRequest final {
  net::test_server::HttpRequest http_request;
};

struct NativeTestServerRawHttpResponse final {
  std::unique_ptr<net::test_server::RawHttpResponse> raw_http_response;
};

class NativeTestServerHandleRequestCallback final {
 public:
  explicit NativeTestServerHandleRequestCallback(
      const jni_zero::JavaRef<jobject>& java_callback)
      : java_callback_(java_callback) {}

  std::unique_ptr<net::test_server::HttpResponse> operator()(
      const net::test_server::HttpRequest& http_request) const;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> java_callback_;
};

}  // namespace cronet

namespace jni_zero {

template <>
jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<cronet::NativeTestServerHttpRequest>(
    JNIEnv* env,
    const cronet::NativeTestServerHttpRequest& input);

template <>
cronet::NativeTestServerRawHttpResponse
FromJniType<cronet::NativeTestServerRawHttpResponse>(
    JNIEnv* env,
    const JavaRef<jobject>& java_raw_http_response);

template <>
std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>
FromJniType<std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>>(
    JNIEnv* env,
    const JavaRef<jobject>& java_handle_request_callback);

}  // namespace jni_zero

// Uses the declarations above, so must come after them.
#include "components/cronet/android/cronet_test_apk_jni/NativeTestServer_jni.h"

namespace jni_zero {

template <>
jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<cronet::NativeTestServerHttpRequest>(
    JNIEnv* env,
    const cronet::NativeTestServerHttpRequest&
        native_test_server_http_request) {
  const auto& http_request = native_test_server_http_request.http_request;
  return cronet::Java_NativeTestServer_createHttpRequest(
      env, http_request.relative_url, http_request.headers,
      http_request.method_string, http_request.all_headers,
      http_request.content);
}

template <>
cronet::NativeTestServerRawHttpResponse
FromJniType<cronet::NativeTestServerRawHttpResponse>(
    JNIEnv* env,
    const JavaRef<jobject>& java_raw_http_response) {
  return {.raw_http_response =
              std::make_unique<net::test_server::RawHttpResponse>(
                  cronet::Java_NativeTestServer_getRawHttpResponseHeaders(
                      env, java_raw_http_response),
                  cronet::Java_NativeTestServer_getRawHttpResponseContents(
                      env, java_raw_http_response))};
}

template <>
std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>
FromJniType<std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>>(
    JNIEnv* env,
    const JavaRef<jobject>& java_handle_request_callback) {
  return std::make_unique<cronet::NativeTestServerHandleRequestCallback>(
      java_handle_request_callback);
}

}  // namespace jni_zero

namespace cronet {

std::unique_ptr<net::test_server::HttpResponse>
NativeTestServerHandleRequestCallback::operator()(
    const net::test_server::HttpRequest& http_request) const {
  return cronet::Java_NativeTestServer_handleRequest(
             jni_zero::AttachCurrentThread(), java_callback_,
             {.http_request = http_request})
      .raw_http_response;
}

}  // namespace cronet

namespace {

const char kSimplePath[] = "/simple";
const char kEchoHeaderPath[] = "/echo_header?";
const char kEchoMethodPath[] = "/echo_method";
const char kEchoAllHeadersPath[] = "/echo_all_headers";
const char kRedirectToEchoBodyPath[] = "/redirect_to_echo_body";
const char kSetCookiePath[] = "/set_cookie?";
const char kUseEncodingPath[] = "/use_encoding?";
const char kEchoBodyPath[] = "/echo_body";

const char kSimpleResponse[] = "The quick brown fox jumps over the lazy dog.";

std::unique_ptr<net::test_server::HttpResponse> SimpleRequest() {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(kSimpleResponse);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> UseEncodingInResponse(
    const net::test_server::HttpRequest& request) {
  DCHECK(base::StartsWith(request.relative_url, kUseEncodingPath,
                          base::CompareCase::INSENSITIVE_ASCII));

  // Each of these is a compression of the string "The quick brown fox jumps
  // over the lazy dog\n".
  std::string_view encoding =
      std::string_view(request.relative_url).substr(strlen(kUseEncodingPath));
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (encoding == "brotli") {
    static const uint8_t kCompressed[] = {
        0x0b, 0x15, 0x80, 0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b,
        0x20, 0x62, 0x72, 0x6f, 0x77, 0x6e, 0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a,
        0x75, 0x6d, 0x70, 0x73, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68,
        0x65, 0x20, 0x6c, 0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67, 0x03};
    http_response->set_content(base::as_string_view(kCompressed));
    http_response->AddCustomHeader("content-encoding", "br");
  } else if (encoding == "dcb") {
    // Contents of a "Dictionary-Compressed Brotli" stream when:
    // * Dictionary = "A dictionary"
    // * Payload = "This is compressed test data using a test dictionary"
    //
    // Accordingly to draft-ietf-httpbis-compression-dictionary-08 the stream is
    // composed of: a header (magic number "ff:44:43:42" & SHA-256 of
    // Dictionary) and the compressed payload.
    //
    // The compressed payload can be obtained via //third-party/brotli:brotli by
    // passing: --dictionary=Dictionary --input=Payload (make sure to be
    // consistent with the presence of EOL).
    static const uint8_t kCompressed[] = {
        0xff, 0x44, 0x43, 0x42, 0x0a, 0xa3, 0x69, 0x01, 0x4f, 0x7f, 0xab,
        0x37, 0x0b, 0xe9, 0x40, 0x74, 0x69, 0x85, 0x45, 0xc7, 0xbb, 0x93,
        0x2e, 0xc4, 0x61, 0x25, 0x27, 0x8f, 0x37, 0xbf, 0x34, 0xab, 0x02,
        0xa3, 0x5a, 0xec, 0xa1, 0x98, 0x01, 0x80, 0x22, 0xe0, 0x26, 0x4b,
        0x95, 0x5c, 0x19, 0x18, 0x9d, 0xc1, 0xc3, 0x44, 0x0e, 0x5c, 0x6a,
        0x09, 0x9d, 0xf0, 0xb0, 0x01, 0x47, 0x14, 0x87, 0x14, 0x6d, 0xfb,
        0x60, 0x96, 0xdb, 0xae, 0x9e, 0x79, 0x54, 0xe3, 0x69, 0x03, 0x29};
    http_response->set_content(base::as_string_view(kCompressed));
    http_response->AddCustomHeader("content-encoding", "dcb");
  } else if (encoding == "zstd") {
    static const uint8_t kCompressed[] = {
        0x28, 0xb5, 0x2f, 0xfd, 0x24, 0x2c, 0x61, 0x01, 0x00, 0x54, 0x68, 0x65,
        0x20, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x20, 0x62, 0x72, 0x6f, 0x77, 0x6e,
        0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a, 0x75, 0x6d, 0x70, 0x73, 0x20, 0x6f,
        0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x6c, 0x61, 0x7a, 0x79,
        0x20, 0x64, 0x6f, 0x67, 0x0a, 0xe4, 0xa7, 0xbc, 0x87};
    http_response->set_content(base::as_string_view(kCompressed));
    http_response->AddCustomHeader("content-encoding", "zstd");
  }
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> SetAndEchoCookieInResponse(
    const net::test_server::HttpRequest& request) {
  std::string cookie_line;
  DCHECK(base::StartsWith(request.relative_url, kSetCookiePath,
                          base::CompareCase::INSENSITIVE_ASCII));
  cookie_line = request.relative_url.substr(strlen(kSetCookiePath));
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(cookie_line);
  http_response->AddCustomHeader("Set-Cookie", cookie_line);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> CronetTestRequestHandler(
    net::EmbeddedTestServer* test_server,
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, kSimplePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SimpleRequest();
  }
  if (base::StartsWith(request.relative_url, kSetCookiePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SetAndEchoCookieInResponse(request);
  }
  if (base::StartsWith(request.relative_url, kUseEncodingPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return UseEncodingInResponse(request);
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type("text/plain");

  if (request.relative_url == kEchoBodyPath) {
    if (request.has_content) {
      response->set_content(request.content);
    } else {
      response->set_content("Request has no body. :(");
    }
    return std::move(response);
  }

  if (base::StartsWith(request.relative_url, kEchoHeaderPath,
                       base::CompareCase::SENSITIVE)) {
    GURL url = test_server->GetURL(request.relative_url);
    auto it = request.headers.find(url.query());
    if (it != request.headers.end()) {
      response->set_content(it->second);
    } else {
      response->set_content("Header not found. :(");
    }
    return std::move(response);
  }

  if (request.relative_url == kEchoAllHeadersPath) {
    response->set_content(request.all_headers);
    return std::move(response);
  }

  if (request.relative_url == kEchoMethodPath) {
    response->set_content(request.method_string);
    return std::move(response);
  }

  if (request.relative_url == kRedirectToEchoBodyPath) {
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader("Location", kEchoBodyPath);
    return std::move(response);
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return nullptr;
}

}  // namespace

namespace cronet {

static long JNI_NativeTestServer_Create(
    JNIEnv* env,
    std::string& test_files_root,
    std::string& test_data_dir,
    bool use_https,
    net::EmbeddedTestServer::ServerCertificate certificate) {
  base::InitAndroidTestPaths(base::FilePath(test_data_dir));
  return reinterpret_cast<long>(new EmbeddedTestServerAdapter(
      base::FilePath(test_files_root),
      (use_https ? net::test_server::EmbeddedTestServer::TYPE_HTTPS
                 : net::test_server::EmbeddedTestServer::TYPE_HTTP),
      certificate));
}

EmbeddedTestServerAdapter::EmbeddedTestServerAdapter(
    const base::FilePath& test_files_root,
    net::EmbeddedTestServer::Type server_type,
    net::EmbeddedTestServer::ServerCertificate server_certificate)
    : test_server(net::EmbeddedTestServer(server_type)) {
  test_server.RegisterRequestHandler(
      base::BindRepeating(&CronetTestRequestHandler, &test_server));
  test_server.ServeFilesFromDirectory(test_files_root);
  net::test_server::RegisterDefaultHandlers(&test_server);
  test_server.SetSSLConfig(server_certificate);
}

EmbeddedTestServerAdapter::~EmbeddedTestServerAdapter() = default;

void EmbeddedTestServerAdapter::EnableConnectProxy(
    JNIEnv* env,
    std::vector<std::string>& urls) {
  std::vector<net::HostPortPair> destinations;
  for (auto& url : urls) {
    destinations.push_back(net::HostPortPair::FromURL(GURL(url)));
  }
  test_server.EnableConnectProxy(destinations);
}

void EmbeddedTestServerAdapter::Destroy(JNIEnv* env) {
  delete this;
}

bool EmbeddedTestServerAdapter::Start(JNIEnv* env) {
  return test_server.Start();
}

int EmbeddedTestServerAdapter::GetPort(JNIEnv* env) {
  return test_server.port();
}

std::string EmbeddedTestServerAdapter::GetHostPort(JNIEnv* env) {
  return net::HostPortPair::FromURL(test_server.base_url()).ToString();
}

std::string EmbeddedTestServerAdapter::GetSimpleURL(JNIEnv* env) {
  return GetFileURL(env, kSimplePath);
}

std::string EmbeddedTestServerAdapter::GetEchoMethodURL(JNIEnv* env) {
  return GetFileURL(env, kEchoMethodPath);
}

std::string EmbeddedTestServerAdapter::GetEchoHeaderURL(
    JNIEnv* env,
    const std::string& header_name) {
  return GetFileURL(env, kEchoHeaderPath + header_name);
}

std::string EmbeddedTestServerAdapter::GetUseEncodingURL(
    JNIEnv* env,
    const std::string& encoding_name) {
  return GetFileURL(env, kUseEncodingPath + encoding_name);
}

std::string EmbeddedTestServerAdapter::GetSetCookieURL(
    JNIEnv* env,
    const std::string& cookie_line) {
  return GetFileURL(env, kSetCookiePath + cookie_line);
}

std::string EmbeddedTestServerAdapter::GetEchoAllHeadersURL(JNIEnv* env) {
  return GetFileURL(env, kEchoAllHeadersPath);
}

std::string EmbeddedTestServerAdapter::GetEchoBodyURL(JNIEnv* env) {
  return GetFileURL(env, kEchoBodyPath);
}

std::string EmbeddedTestServerAdapter::GetRedirectToEchoBodyURL(JNIEnv* env) {
  return GetFileURL(env, kRedirectToEchoBodyPath);
}

std::string EmbeddedTestServerAdapter::GetExabyteResponseURL(JNIEnv* env) {
  return GetFileURL(env, "/exabyte_response");
}

std::string EmbeddedTestServerAdapter::GetFileURL(
    JNIEnv* env,
    const std::string& file_path) {
  return test_server.GetURL(file_path).spec();
}

void EmbeddedTestServerAdapter::RegisterRequestHandler(
    JNIEnv* env,
    std::unique_ptr<NativeTestServerHandleRequestCallback>& callback) {
  test_server.RegisterRequestHandler(
      base::BindRepeating(&NativeTestServerHandleRequestCallback::operator(),
                          base::Owned(std::move(callback))));
}

}  // namespace cronet

DEFINE_JNI(NativeTestServer)
