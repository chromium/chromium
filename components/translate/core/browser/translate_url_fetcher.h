// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_URL_FETCHER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_URL_FETCHER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}

namespace translate {

class TranslateUrlFetcher {
 public:
  using Callback = base::OnceCallback<void(bool, const std::string&)>;
  // Represents internal state if the fetch is completed successfully.
  enum State {
    IDLE,        // No fetch request was issued.
    REQUESTING,  // A fetch request was issued, but not finished yet.
    COMPLETED,   // The last fetch request was finished successfully.
    FAILED,      // The last fetch request was finished with a failure.
  };

  virtual ~TranslateUrlFetcher() = default;

  // Requests to |url|. |callback| will be invoked when the function returns
  // true, and the request is finished asynchronously.
  // Returns false if the previous request is not finished, or the request
  // is omitted due to retry limitation. |is_incognito| is used during the
  // fetch to determine which variations headers to add.
  virtual bool Request(const GURL& url,
                       Callback callback,
                       bool is_incognito) = 0;

  // Returns the internal state.
  virtual State state() const = 0;
};

// Downloads raw Translate data such as the Translate script and the language
// list.
class TranslateURLFetcherImpl : public TranslateUrlFetcher {
 public:
  explicit TranslateURLFetcherImpl(int max_retry_on_5xx);
  explicit TranslateURLFetcherImpl(
      const net::HttpRequestHeaders& extra_request_header);

  TranslateURLFetcherImpl(const TranslateURLFetcherImpl&) = delete;
  TranslateURLFetcherImpl& operator=(const TranslateURLFetcherImpl&) = delete;

  ~TranslateURLFetcherImpl() override;

  // TranslateUrlFetcher implementation.
  bool Request(const GURL& url, Callback callback, bool is_incognito) override;
  State state() const override;

 private:
  void OnSimpleLoaderComplete(std::optional<std::string> response_body);

  // URL to send the request.
  GURL url_;

  // Internal state.
  enum State state_;

  // SimpleURLLoader instance.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // Callback passed at Request(). It will be invoked when an asynchronous
  // load operation is finished.
  Callback callback_;

  // Counts how many times did it try to load the language list.
  int retry_count_;

  // Max number how many times to retry on the server error
  int max_retry_on_5xx_;

  // An extra HTTP request header
  net::HttpRequestHeaders extra_request_header_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_URL_FETCHER_H_
