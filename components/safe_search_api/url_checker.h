// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_
#define COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_

#include <list>
#include <memory>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/time/time.h"
#include "components/safe_search_api/url_checker_client.h"
#include "url/gurl.h"

namespace base {
struct Feature;
}

namespace safe_search_api {

// The SafeSearch API classification of a URL.
enum class Classification { SAFE, UNSAFE };

// Visible for testing.
extern const base::Feature kAllowAllGoogleUrls;

// This class uses one implementation of URLCheckerClient to check the
// classification of the content on a given URL and returns the result
// asynchronously via a callback. It is also responsible for the synchronous
// logic such as caching, the injected URLCheckerClient is who makes the
// async request.
class URLChecker {
 public:
  // Used to report whether |url| should be blocked. Called from CheckURL.
  using CheckCallback = base::OnceCallback<
      void(const GURL&, Classification classification, bool /* uncertain */)>;

  explicit URLChecker(std::unique_ptr<URLCheckerClient> async_checker);

  URLChecker(std::unique_ptr<URLCheckerClient> async_checker,
             size_t cache_size);

  ~URLChecker();

  // Returns whether |callback| was run synchronously.
  bool CheckURL(const GURL& url, CheckCallback callback);

  void SetCacheTimeoutForTesting(const base::TimeDelta& timeout) {
    cache_timeout_ = timeout;
  }

 private:
  struct Check;
  struct CheckResult {
    CheckResult(Classification classification, bool uncertain);
    Classification classification;
    bool uncertain;
    base::TimeTicks timestamp;
  };
  using CheckList = std::list<std::unique_ptr<Check>>;

  void OnAsyncCheckComplete(CheckList::iterator it,
                            const GURL& url,
                            ClientClassification classification);

  std::unique_ptr<URLCheckerClient> async_checker_;
  CheckList checks_in_progress_;

  base::MRUCache<GURL, CheckResult> cache_;
  base::TimeDelta cache_timeout_;

  DISALLOW_COPY_AND_ASSIGN(URLChecker);
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_
