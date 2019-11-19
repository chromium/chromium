// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "net/http/http_request_headers.h"

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

extern const char kSessionHeaderOption[];
extern const char kCredentialsHeaderOption[];
extern const char kSecureSessionHeaderOption[];
extern const char kBuildNumberHeaderOption[];
extern const char kPatchNumberHeaderOption[];
extern const char kClientHeaderOption[];

#if defined(OS_ANDROID)
extern const char kAndroidWebViewProtocolVersion[];
#endif

class DataReductionProxyConfig;

typedef base::RepeatingCallback<void(const net::HttpRequestHeaders&)>
    UpdateHeaderCallback;

class DataReductionProxyRequestOptions {
 public:
  static bool IsKeySetOnCommandLine();

  // Constructs a DataReductionProxyRequestOptions object with the given
  // client type, and config.
  DataReductionProxyRequestOptions(Client client,
                                   DataReductionProxyConfig* config);

  virtual ~DataReductionProxyRequestOptions();

  // Sets |key_| to the default key and initializes the credentials, version,
  // client, and lo-fi header values. Generates the |header_value_| string,
  // which is concatenated to the Chrome-proxy header. Called on the UI thread.
  void Init();

  // Adds a 'Chrome-Proxy' header to |request_headers| with the data reduction
  // proxy authentication credentials. |page_id| should only be non-empty for
  // main frame requests.
  void AddRequestHeader(net::HttpRequestHeaders* request_headers,
                        base::Optional<uint64_t> page_id);
  static void AddRequestHeader(net::HttpRequestHeaders* request_headers,
                               base::Optional<uint64_t> page_id,
                               const std::string& session_header_value);

  // Adds |page_id| to the 'Chrome-Proxy' header, merging with existing value if
  // it exists.
  static void AddPageIDRequestHeader(net::HttpRequestHeaders* request_headers,
                                     uint64_t page_id);

  // Stores the supplied key and sets up credentials suitable for authenticating
  // with the data reduction proxy.
  // This can be called more than once. For example on a platform that does not
  // have a default key defined, this function will be called some time after
  // this class has been constructed. Android WebView is a platform that does
  // this. The caller needs to make sure |this| pointer is valid when
  // SetKey is called.
  void SetKeyForTesting(const std::string& key);

  // Sets the credentials for sending to the Data Reduction Proxy.
  void SetSecureSession(const std::string& secure_session);

  // Set the callback to call when the proxy request headers are updated.
  void SetUpdateHeaderCallback(UpdateHeaderCallback callback) {
    update_header_callback_ = callback;
  }

  // Retrieves the credentials for sending to the Data Reduction Proxy.
  const std::string& GetSecureSession() const;

  // Invalidates the secure session credentials.
  void Invalidate();

  // Parses |request_headers| and returns the value of the session key.
  static base::Optional<std::string> GetSessionKeyFromRequestHeaders(
      const net::HttpRequestHeaders& request_headers);

  // Parses |request_headers| and returns the value of the page id.
  static base::Optional<uint64_t> GetPageIdFromRequestHeaders(
      const net::HttpRequestHeaders& request_headers);

  // Creates and returns a new unique page ID (unique per session).
  uint64_t GeneratePageId();

 protected:
  // Visible for testing.
  virtual std::string GetDefaultKey() const;

  // Visible for testing.
  DataReductionProxyRequestOptions(Client client,
                                   const std::string& version,
                                   DataReductionProxyConfig* config);

  // Returns the chrome proxy header. Protected so that it is available for
  // testing.
  std::string GetHeaderValueForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyRequestOptionsTest,
                           AuthHashForSalt);

  // Resets the page ID for a new session.
  // TODO(ryansturm): Create a session object to store this and other data saver
  // session info. crbug.com/709624
  void ResetPageId();

  // Generates and updates the session ID and credentials.
  void UpdateCredentials();

  // Regenerates the |header_value_| string which is concatenated to the
  // Chrome-proxy header.
  void RegenerateRequestHeaderValue();

  // The Chrome-Proxy header value.
  std::string header_value_;

  // Authentication state.
  std::string key_;

  // Name of the client and version of the data reduction proxy protocol to use.
  const std::string client_;
  std::string secure_session_;
  std::string build_;
  std::string patch_;
  const std::string server_experiments_;

  // Must outlive |this|.
  DataReductionProxyConfig* data_reduction_proxy_config_;

  // The page identifier that was last generated for data saver proxy server.
  uint64_t current_page_id_;

  // Callback to expose the chrome_proxy header to the UI thread. Called
  // whenever the chrome_proxy header value changes. Can be null.
  UpdateHeaderCallback update_header_callback_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyRequestOptions);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_AUTH_REQUEST_HANDLER_H_
