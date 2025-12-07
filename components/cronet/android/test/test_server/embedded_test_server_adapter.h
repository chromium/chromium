// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_TEST_TEST_SERVER_EMBEDDED_TEST_SERVER_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_TEST_TEST_SERVER_EMBEDDED_TEST_SERVER_ADAPTER_H_

#include <string>

#include "net/test/embedded_test_server/embedded_test_server.h"

namespace base {
class FilePath;
}  // namespace base

namespace cronet {

class NativeTestServerHandleRequestCallback;

class EmbeddedTestServerAdapter {
 public:
  EmbeddedTestServerAdapter(
      const base::FilePath& test_files_root,
      net::EmbeddedTestServer::Type server_type,
      net::EmbeddedTestServer::ServerCertificate server_certificate);

  ~EmbeddedTestServerAdapter();

  void Destroy(JNIEnv* env);

  // Starts the server serving files from default test data directory.
  // Returns true if started, false if server is already running.
  // This will run the server in default mode (HTTP/1 with no SSL)
  bool Start(JNIEnv* env);

  void EnableConnectProxy(JNIEnv* env, std::vector<std::string>& urls);

  // Returns port number of the server.
  int GetPort(JNIEnv* env);
  // Returns host:port string of the server.
  std::string GetHostPort(JNIEnv* env);

  // Returns URL which responds with the body "The quick brown fox jumps over
  // the lazy dog".
  std::string GetSimpleURL(JNIEnv* env);
  // Returns URL which respond with echo of the method in response body.
  std::string GetEchoMethodURL(JNIEnv* env);
  // Returns URL which respond with echo of header with |header_name| in
  // response body.
  std::string GetEchoHeaderURL(JNIEnv* env, const std::string& header_name);
  // Returns URL which responds with "The quick brown fox jumps over the lazy
  // dog" in specified encoding.
  std::string GetUseEncodingURL(JNIEnv* env, const std::string& encoding_name);
  // Returns URL which respond with setting cookie to |cookie_line| and echo it
  // in response body.
  std::string GetSetCookieURL(JNIEnv* env, const std::string& cookie_line);
  // Returns URL which echoes all request headers.
  std::string GetEchoAllHeadersURL(JNIEnv* env);
  // Returns URL which echoes data in a request body.
  std::string GetEchoBodyURL(JNIEnv* env);
  // Returns URL which redirects to URL that echoes data in a request body.
  std::string GetRedirectToEchoBodyURL(JNIEnv* env);
  // Returns a URL that the server will return an Exabyte of data.
  std::string GetExabyteResponseURL(JNIEnv* env);

  // The following URLs will make EmbeddedTestServerAdapter serve a response
  // based on the contents of the corresponding file and its mock-http-headers
  // file.

  // Returns URL which responds with content of file at |file_path|.
  std::string GetFileURL(JNIEnv* env, const std::string& file_path);

  // Returns URL which responds with plain/text success.
  std::string GetSuccessURL(JNIEnv* env) {
    return GetFileURL(env, "/success.txt");
  }

  // Returns URL which redirects to plain/text success.
  std::string GetRedirectURL(JNIEnv* env) {
    return GetFileURL(env, "/redirect.html");
  }

  // Returns URL which redirects to redirect to plain/text success.
  std::string GetMultiRedirectURL(JNIEnv* env) {
    return GetFileURL(env, "/multiredirect.html");
  }

  // Returns URL which responds with status code 404 - page not found..
  std::string GetNotFoundURL(JNIEnv* env) {
    return GetFileURL(env, "/notfound.html");
  }

  // See net::test_server::EmbeddedTestServer::RegisterRequestHandler()
  void RegisterRequestHandler(
      JNIEnv* env,
      std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>& callback);

 private:
  net::test_server::EmbeddedTestServer test_server;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_TEST_TEST_SERVER_EMBEDDED_TEST_SERVER_ADAPTER_H_
