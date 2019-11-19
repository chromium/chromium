// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/test/test_server.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
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
    FILE_PATH_LITERAL("components/cronet/test/data");

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
  std::string encoding;
  DCHECK(base::StartsWith(request.relative_url, kUseEncodingPath,
                          base::CompareCase::INSENSITIVE_ASCII));

  encoding = request.relative_url.substr(strlen(kUseEncodingPath));
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (!encoding.compare("brotli")) {
    const char quickfoxCompressed[] = {
        0x0b, 0x15, -0x80, 0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b,
        0x20, 0x62, 0x72,  0x6f, 0x77, 0x6e, 0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a,
        0x75, 0x6d, 0x70,  0x73, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68,
        0x65, 0x20, 0x6c,  0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67, 0x03};
    std::string quickfoxCompressedStr(quickfoxCompressed,
                                      sizeof(quickfoxCompressed));
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(quickfoxCompressedStr);
    http_response->AddCustomHeader(std::string("content-encoding"),
                                   std::string("br"));
  }
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> ReturnBigDataInResponse(
    const net::test_server::HttpRequest& request) {
  DCHECK(base::StartsWith(request.relative_url, kBigDataPath,
                          base::CompareCase::INSENSITIVE_ASCII));
  std::string data_size_str = request.relative_url.substr(strlen(kBigDataPath));
  int64_t data_size;
  CHECK(base::StringToInt64(base::StringPiece(data_size_str), &data_size));
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
  return std::unique_ptr<net::test_server::BasicHttpResponse>();
}

}  // namespace

namespace cronet {

/* static */
bool TestServer::StartServeFilesFromDirectory(
    const base::FilePath& test_files_root) {
  // Shouldn't happen.
  if (g_test_server)
    return false;

  g_test_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  g_test_server->RegisterRequestHandler(
      base::BindRepeating(&CronetTestRequestHandler));
  g_test_server->ServeFilesFromDirectory(test_files_root);
  net::test_server::RegisterDefaultHandlers(g_test_server.get());
  CHECK(g_test_server->Start());
  return true;
}

/* static */
bool TestServer::Start() {
  base::FilePath src_root;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root));
  return StartServeFilesFromDirectory(src_root.Append(kTestDataRelativePath));
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

}  // namespace cronet
