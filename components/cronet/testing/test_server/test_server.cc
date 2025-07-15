// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/cronet/testing/test_server/test_server.h"

#include <memory>
#include <string_view>
#include <utility>

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
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

// Cronet test data directory, relative to source root.
const base::FilePath::CharType kTestDataRelativePath[] =
    FILE_PATH_LITERAL("components/cronet/testing/test_server/data");

const char kSimplePath[] = "/simple";
const char kEchoHeaderPath[] = "/echo_header?";
const char kEchoMethodPath[] = "/echo_method";
const char kEchoAllHeadersPath[] = "/echo_all_headers";
const char kRedirectToEchoBodyPath[] = "/redirect_to_echo_body";
const char kSetCookiePath[] = "/set_cookie?";
const char kBigDataPath[] = "/big_data?";
const char kUseEncodingPath[] = "/use_encoding?";
const char kEchoBodyPath[] = "/echo_body";

const char kSimpleResponse[] = "The quick brown fox jumps over the lazy dog.";

std::unique_ptr<net::EmbeddedTestServer> g_test_server;
base::LazyInstance<std::string>::Leaky g_big_data_body =
    LAZY_INSTANCE_INITIALIZER;

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

std::unique_ptr<net::test_server::HttpResponse> ReturnBigDataInResponse(
    const net::test_server::HttpRequest& request) {
  DCHECK(base::StartsWith(request.relative_url, kBigDataPath,
                          base::CompareCase::INSENSITIVE_ASCII));
  std::string data_size_str = request.relative_url.substr(strlen(kBigDataPath));
  int64_t data_size;
  CHECK(base::StringToInt64(std::string_view(data_size_str), &data_size));
  CHECK(data_size == static_cast<int64_t>(g_big_data_body.Get().size()));
  return std::make_unique<net::test_server::RawHttpResponse>(
      std::string(), g_big_data_body.Get());
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
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, kSimplePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SimpleRequest();
  }
  if (base::StartsWith(request.relative_url, kSetCookiePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SetAndEchoCookieInResponse(request);
  }
  if (base::StartsWith(request.relative_url, kBigDataPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ReturnBigDataInResponse(request);
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
    GURL url = g_test_server->GetURL(request.relative_url);
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

/* static */
bool TestServer::PrepareServeFilesFromDirectory(
    const base::FilePath& test_files_root,
    net::EmbeddedTestServer::Type server_type,
    net::EmbeddedTestServer::ServerCertificate server_certificate) {
  // Shouldn't happen.
  if (g_test_server)
    return false;

  g_test_server = std::make_unique<net::EmbeddedTestServer>(server_type);
  g_test_server->RegisterRequestHandler(
      base::BindRepeating(&CronetTestRequestHandler));
  g_test_server->ServeFilesFromDirectory(test_files_root);
  net::test_server::RegisterDefaultHandlers(g_test_server.get());
  g_test_server->SetSSLConfig(server_certificate);
  return true;
}

void TestServer::EnableConnectProxy(std::vector<std::string>& urls) {
  std::vector<net::HostPortPair> destinations;
  for (auto& url : urls) {
    destinations.push_back(net::HostPortPair::FromURL(GURL(url)));
  }
  g_test_server->EnableConnectProxy(destinations);
}

void TestServer::StartPrepared() {
  CHECK(g_test_server);
  CHECK(g_test_server->Start());
}

/* static */
bool TestServer::Start() {
  base::FilePath src_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));
  return StartServeFilesFromDirectory(
      src_root.Append(kTestDataRelativePath),
      net::test_server::EmbeddedTestServer::TYPE_HTTP,
      net::test_server::EmbeddedTestServer::CERT_OK);
}

/* static */
void TestServer::Shutdown() {
  if (!g_test_server)
    return;
  g_test_server.reset();
}

/* static */
int TestServer::GetPort() {
  DCHECK(g_test_server);
  return g_test_server->port();
}

/* static */
std::string TestServer::GetHostPort() {
  DCHECK(g_test_server);
  return net::HostPortPair::FromURL(g_test_server->base_url()).ToString();
}

/* static */
std::string TestServer::GetSimpleURL() {
  return GetFileURL(kSimplePath);
}

/* static */
std::string TestServer::GetEchoMethodURL() {
  return GetFileURL(kEchoMethodPath);
}

/* static */
std::string TestServer::GetEchoHeaderURL(const std::string& header_name) {
  return GetFileURL(kEchoHeaderPath + header_name);
}

/* static */
std::string TestServer::GetUseEncodingURL(const std::string& encoding_name) {
  return GetFileURL(kUseEncodingPath + encoding_name);
}

/* static */
std::string TestServer::GetSetCookieURL(const std::string& cookie_line) {
  return GetFileURL(kSetCookiePath + cookie_line);
}

/* static */
std::string TestServer::GetEchoAllHeadersURL() {
  return GetFileURL(kEchoAllHeadersPath);
}

/* static */
std::string TestServer::GetEchoRequestBodyURL() {
  return GetFileURL(kEchoBodyPath);
}

/* static */
std::string TestServer::GetRedirectToEchoBodyURL() {
  return GetFileURL(kRedirectToEchoBodyPath);
}

/* static */
std::string TestServer::GetExabyteResponseURL() {
  return GetFileURL("/exabyte_response");
}

/* static */
std::string TestServer::PrepareBigDataURL(size_t data_size) {
  DCHECK(g_test_server);
  DCHECK(g_big_data_body.Get().empty());
  // Response line with headers.
  std::string response_builder;
  base::StringAppendF(&response_builder, "HTTP/1.1 200 OK\r\n");
  base::StringAppendF(&response_builder, "Content-Length: %" PRIuS "\r\n",
                      data_size);
  base::StringAppendF(&response_builder, "\r\n");
  response_builder += std::string(data_size, 'c');
  g_big_data_body.Get() = response_builder;
  return g_test_server
      ->GetURL(kBigDataPath + base::NumberToString(response_builder.size()))
      .spec();
}

/* static */
void TestServer::ReleaseBigDataURL() {
  DCHECK(!g_big_data_body.Get().empty());
  g_big_data_body.Get() = std::string();
}

/* static */
std::string TestServer::GetFileURL(const std::string& file_path) {
  DCHECK(g_test_server);
  return g_test_server->GetURL(file_path).spec();
}

void TestServer::RegisterRequestHandler(
    net::test_server::EmbeddedTestServer::HandleRequestCallback callback) {
  CHECK(g_test_server);
  g_test_server->RegisterRequestHandler(callback);
}

}  // namespace cronet
