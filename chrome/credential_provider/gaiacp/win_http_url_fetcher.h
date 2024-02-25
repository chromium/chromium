// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_WIN_HTTP_URL_FETCHER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_WIN_HTTP_URL_FETCHER_H_

#include <map>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace credential_provider {

class FakeWinHttpUrlFetcherFactory;

// A synchronous URL fetcher for small requests.
class WinHttpUrlFetcher {
 public:
  static std::unique_ptr<WinHttpUrlFetcher> Create(const GURL& url);

  // Builds the required json request to be sent to the http service and fetches
  // the json response from the service (if any). Returns S_OK if retrieved a
  // proper json response from the service, otherwise returns an error code. The
  // json response is written into |request_result|. |request_url| is the full
  // query url from which to fetch a response. |headers| are all the header key
  // value pairs to be sent with the request. |request_dict| is a dictionary of
  // json parameters to be sent with the request. This argument will be
  // converted to a json string and sent as the body of the request.
  // |request_timeout| is the maximum time to wait for a response. If the HTTP
  // request times out or fails with an error response code that signifies an
  // internal server error (HTTP codes >= 500) then the request will be retried
  // |request_retries| number of times before the call is marked failed.
  static HRESULT BuildRequestAndFetchResultFromHttpService(
      const GURL& request_url,
      std::string access_token,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const base::Value::Dict& request_dict,
      const base::TimeDelta& request_timeout,
      unsigned int request_retries,
      std::optional<base::Value>* request_result);

  virtual ~WinHttpUrlFetcher();

  virtual bool IsValid() const;

  virtual HRESULT SetRequestHeader(const char* name, const char* value);
  virtual HRESULT SetRequestBody(const char* body);
  virtual HRESULT SetHttpRequestTimeout(const int timeout_in_millis);
  virtual HRESULT Fetch(std::vector<char>* response);
  virtual HRESULT Close();

  using CreatorFunc = decltype(Create);
  using CreatorCallback = base::RepeatingCallback<CreatorFunc>;

  // Set the creator callback function to use in tests.
  static void SetCreatorForTesting(CreatorCallback creator);

 protected:
  using Headers = std::map<std::string, std::string>;

  // This constructor is used by the derived fake class to bypass the
  // initialization code in the public constructor that will fail because the
  // tests are not running elevated.
  WinHttpUrlFetcher();

 private:
  friend class FakeWinHttpUrlFetcherFactory;

  explicit WinHttpUrlFetcher(const GURL& url);

  GURL url_;
  Headers request_headers_;
  std::string body_;
  ScopedWinHttpHandle session_;
  ScopedWinHttpHandle request_;
  int timeout_in_millis_ = 0;

  // Gets storage of the function pointer used to create instances of this
  // class for tests.
  static CreatorCallback* GetCreatorFunctionStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_WIN_HTTP_URL_FETCHER_H_
