// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_THREAT_DETAILS_HISTORY_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_THREAT_DETAILS_HISTORY_H_

// This class gets redirect chain for urls from the history service.

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

typedef std::vector<GURL> RedirectChain;

class ThreatDetailsRedirectsCollector : public history::HistoryServiceObserver {
 public:
  explicit ThreatDetailsRedirectsCollector(
      const base::WeakPtr<history::HistoryService>& history_service);
  ~ThreatDetailsRedirectsCollector() override;

  ThreatDetailsRedirectsCollector(const ThreatDetailsRedirectsCollector&) =
      delete;
  ThreatDetailsRedirectsCollector& operator=(
      const ThreatDetailsRedirectsCollector&) = delete;

  // Collects urls' redirects chain information from the history service.
  // We get access to history service via web_contents in UI thread.
  void StartHistoryCollection(const std::vector<GURL>& urls,
                              base::OnceClosure callback);

  // Returns whether or not StartCacheCollection has been called.
  bool HasStarted() const;

  // Returns the redirect urls we get from history service
  const std::vector<RedirectChain>& GetCollectedUrls() const;

  // history::HistoryServiceObserver
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  friend class base::RefCounted<ThreatDetailsRedirectsCollector>;

  void StartGetRedirects(const std::vector<GURL>& urls);
  void GetRedirects(const GURL& url);
  void OnGotQueryRedirectsTo(const GURL& url,
                             history::RedirectList redirect_list);

  // Runs the callback when redirects collecting is all done.
  void AllDone();

  base::CancelableTaskTracker request_tracker_;

  // Method we call when we are done. The caller must be alive for the
  // whole time, we are modifying its state (see above).
  base::OnceClosure callback_;

  // Sets to true once StartHistoryCollection is called
  bool has_started_;

  // The urls we need to get redirects for
  std::vector<GURL> urls_;
  // The iterator goes over urls_
  std::vector<GURL>::iterator urls_it_;
  // The collected directs from history service
  std::vector<RedirectChain> redirects_urls_;

  base::WeakPtr<history::HistoryService> history_service_;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<ThreatDetailsRedirectsCollector> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_THREAT_DETAILS_HISTORY_H_
