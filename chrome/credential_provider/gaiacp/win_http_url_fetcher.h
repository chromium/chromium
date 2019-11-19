// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_WIN_HTTP_URL_FETCHER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_WIN_HTTP_URL_FETCHER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "chrome/credential_provider/gaiacp/scoped_handle.h"
#include "url/gurl.h"

namespace credential_provider {

class FakeWinHttpUrlFetcherFactory;

// A synchronous URL fetcher for small requests.
class WinHttpUrlFetcher {
 public:
  static std::unique_ptr<WinHttpUrlFetcher> Create(const GURL& url);

  virtual ~WinHttpUrlFetcher();

  virtual bool IsValid() const;

  virtual HRESULT SetRequestHeader(const char* name, const char* value);
  virtual HRESULT SetRequestBody(const char* body);
  virtual HRESULT SetHttpRequestTimeout(const int timeout_in_millis);
  virtual HRESULT Fetch(std::vector<char>* response);
  virtual HRESULT Close();

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
  using CreatorFunc = decltype(Create);
  using CreatorCallback = base::RepeatingCallback<CreatorFunc>;
  static CreatorCallback* GetCreatorFunctionStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_WIN_HTTP_URL_FETCHER_H_
