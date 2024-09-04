// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_
#define COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_

#include <list>
#include <memory>

#include "base/containers/lru_cache.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/safe_search_api/url_checker_client.h"
#include "url/gurl.h"

namespace safe_search_api {

// The SafeSearch API classification of a URL.
enum class Classification { SAFE, UNSAFE };

// Additional details regarding how the `Classification` result was determined.
struct ClassificationDetails {
  enum class Reason {
    // Chrome sent a fresh external request to the server, and received a valid
    // response.
    kFreshServerResponse,

    // Chrome used a cached response, stored from a previous successful request
    // to the server.
    kCachedResponse,

    // Chrome tried but failed to query the server, and defaulted to
    // `Classification::SAFE`.
    kFailedUseDefault,
  };
  Reason reason;
};

// These values are sent to Uma to understand Cache utilization. Must not be
// renumbered.
enum class CacheAccessStatus {
  // Entry was found.
  kHit = 0,
  // Entry was not found.
  kNotFound = 1,
  // Entry was found but was stale.
  kOutdated = 2,
  kMaxValue = kOutdated,
};

// This class uses one implementation of URLCheckerClient to check the
// classification of the content on a given URL and returns the result
// asynchronously via a callback. It is also responsible for the synchronous
// logic such as caching, the injected URLCheckerClient is who makes the
// async request.
class URLChecker {
 public:
  // Used to report whether |url| should be blocked. Called from CheckURL.
  using CheckCallback = base::OnceCallback<void(const GURL&,
                                                Classification classification,
                                                ClassificationDetails details)>;

  explicit URLChecker(std::unique_ptr<URLCheckerClient> async_checker);

  URLChecker(std::unique_ptr<URLCheckerClient> async_checker,
             size_t cache_size);

  URLChecker(const URLChecker&) = delete;
  URLChecker& operator=(const URLChecker&) = delete;

  ~URLChecker();

  // Returns whether |callback| was run synchronously.
  bool CheckURL(const GURL& url, CheckCallback callback);

  void SetCacheTimeoutForTesting(const base::TimeDelta& timeout) {
    cache_timeout_ = timeout;
  }

 private:
  struct Check;
  struct CheckResult {
    CheckResult() = delete;
    explicit CheckResult(Classification classification);
    Classification classification;
    base::TimeTicks timestamp;
  };
  using CheckList = std::list<std::unique_ptr<Check>>;

  void OnAsyncCheckComplete(CheckList::iterator it,
                            const GURL& url,
                            ClientClassification classification);

  // Either reuses pending check for given url if it already exists (see
  // checks_in_progress_), or schedules a new one.
  void MaybeScheduleAsyncCheck(const GURL& url, CheckCallback callback);

  std::unique_ptr<URLCheckerClient> async_checker_;
  CheckList checks_in_progress_;

  base::LRUCache<GURL, CheckResult> cache_;
  base::TimeDelta cache_timeout_;

  base::WeakPtrFactory<URLChecker> weak_factory_{this};
};

}  // namespace safe_search_api

#endif  // COMPONENTS_SAFE_SEARCH_API_URL_CHECKER_H_
