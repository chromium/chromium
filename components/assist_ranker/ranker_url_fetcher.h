// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_RANKER_URL_FETCHER_H_
#define COMPONENTS_ASSIST_RANKER_RANKER_URL_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace assist_ranker {

// Downloads Ranker models.
class RankerURLFetcher {
 public:
  // Callback type for Request().
  typedef base::OnceCallback<void(bool, const std::string&)> Callback;

  // Represents internal state if the fetch is completed successfully.
  enum State {
    IDLE,        // No fetch request was issued.
    REQUESTING,  // A fetch request was issued, but not finished yet.
    COMPLETED,   // The last fetch request was finished successfully.
    FAILED,      // The last fetch request was finished with a failure.
  };

  RankerURLFetcher();

  RankerURLFetcher(const RankerURLFetcher&) = delete;
  RankerURLFetcher& operator=(const RankerURLFetcher&) = delete;

  ~RankerURLFetcher();

  int max_retry_on_5xx() { return max_retry_on_5xx_; }
  void set_max_retry_on_5xx(int count) { max_retry_on_5xx_ = count; }

  // Requests to |url|. |callback| will be invoked when the function returns
  // true, and the request is finished asynchronously.
  // Returns false if the previous request is not finished, or the request
  // is omitted due to retry limitation.
  bool Request(const GURL& url,
               Callback callback,
               network::mojom::URLLoaderFactory* url_loader_factory);

  // Gets internal state.
  State state() { return state_; }

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // URL to send the request.
  GURL url_;

  // Internal state.
  enum State state_;

  // SimpleURLLoader instance.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Callback passed at Request(). It will be invoked when an asynchronous
  // fetch operation is finished.
  Callback callback_;

  // Counts how many times did it try to fetch the language list.
  int retry_count_;

  // Max number how many times to retry on the server error
  int max_retry_on_5xx_;
};

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_RANKER_URL_FETCHER_H_
