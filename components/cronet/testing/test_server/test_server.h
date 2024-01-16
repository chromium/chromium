// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_TESTING_TEST_SERVER_TEST_SERVER_H_
#define COMPONENTS_CRONET_TESTING_TEST_SERVER_TEST_SERVER_H_

#include <string>

#include "net/test/embedded_test_server/embedded_test_server.h"

namespace base {
class FilePath;
}  // namespace base

namespace cronet {

class TestServer {
 public:
  // Starts the server serving files from default test data directory.
  // Returns true if started, false if server is already running.
  // This will run the server in default mode (HTTP/1 with no SSL)
  static bool Start();

  // Starts the server serving files from |test_files_root| directory.
  // Returns true if started, false if server is already running.
  // The provided server will support either HTTP/1 or HTTPS/1 depending
  // on the |type| provided.
  static bool StartServeFilesFromDirectory(
      const base::FilePath& test_files_root,
      net::EmbeddedTestServer::Type type,
      net::EmbeddedTestServer::ServerCertificate cert);

  // Shuts down the server.
  static void Shutdown();

  // Returns port number of the server.
  static int GetPort();
  // Returns host:port string of the server.
  static std::string GetHostPort();

  // Returns URL which responds with the body "The quick brown fox jumps over
  // the lazy dog".
  static std::string GetSimpleURL();
  // Returns URL which respond with echo of the method in response body.
  static std::string GetEchoMethodURL();
  // Returns URL which respond with echo of header with |header_name| in
  // response body.
  static std::string GetEchoHeaderURL(const std::string& header_name);
  // Returns URL which responds with "The quick brown fox jumps over the lazy
  // dog" in specified encoding.
  static std::string GetUseEncodingURL(const std::string& encoding_name);
  // Returns URL which respond with setting cookie to |cookie_line| and echo it
  // in response body.
  static std::string GetSetCookieURL(const std::string& cookie_line);
  // Returns URL which echoes all request headers.
  static std::string GetEchoAllHeadersURL();
  // Returns URL which echoes data in a request body.
  static std::string GetEchoRequestBodyURL();
  // Returns URL which redirects to URL that echoes data in a request body.
  static std::string GetRedirectToEchoBodyURL();
  // Returns a URL that the server will return an Exabyte of data.
  static std::string GetExabyteResponseURL();
  // Prepares response and returns URL which respond with |data_size| of bytes
  // in response body.
  static std::string PrepareBigDataURL(size_t data_size);
  // Releases response created by PrepareBigDataURL().
  static void ReleaseBigDataURL();

  // The following URLs will make TestServer serve a response based on
  // the contents of the corresponding file and its mock-http-headers file.

  // Returns URL which responds with content of file at |file_path|.
  static std::string GetFileURL(const std::string& file_path);

  // Returns URL which responds with plain/text success.
  static std::string GetSuccessURL() { return GetFileURL("/success.txt"); }

  // Returns URL which redirects to plain/text success.
  static std::string GetRedirectURL() { return GetFileURL("/redirect.html"); }

  // Returns URL which redirects to redirect to plain/text success.
  static std::string GetMultiRedirectURL() {
    return GetFileURL("/multiredirect.html");
  }

  // Returns URL which responds with status code 404 - page not found..
  static std::string GetNotFoundURL() { return GetFileURL("/notfound.html"); }
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_TESTING_TEST_SERVER_TEST_SERVER_H_
