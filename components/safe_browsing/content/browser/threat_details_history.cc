// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the ThreatDetailsRedirectsCollector class.

#include "components/safe_browsing/content/browser/threat_details_history.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/safe_browsing/content/browser/threat_details.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

ThreatDetailsRedirectsCollector::ThreatDetailsRedirectsCollector(
    const base::WeakPtr<history::HistoryService>& history_service)
    : has_started_(false), history_service_(history_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (history_service) {
    history_service_observation_.Observe(history_service.get());
  }
}

void ThreatDetailsRedirectsCollector::StartHistoryCollection(
    const std::vector<GURL>& urls,
    base::OnceClosure callback) {
  DVLOG(1) << "Num of urls to check in history service: " << urls.size();
  has_started_ = true;
  callback_ = std::move(callback);

  if (urls.size() == 0) {
    AllDone();
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ThreatDetailsRedirectsCollector::StartGetRedirects,
                     weak_factory_.GetWeakPtr(), urls));
}

bool ThreatDetailsRedirectsCollector::HasStarted() const {
  return has_started_;
}

const std::vector<RedirectChain>&
ThreatDetailsRedirectsCollector::GetCollectedUrls() const {
  return redirects_urls_;
}

ThreatDetailsRedirectsCollector::~ThreatDetailsRedirectsCollector() {}

void ThreatDetailsRedirectsCollector::StartGetRedirects(
    const std::vector<GURL>& urls) {
  // History access from profile needs to happen in UI thread
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (size_t i = 0; i < urls.size(); ++i) {
    urls_.push_back(urls[i]);
  }
  urls_it_ = urls_.begin();
  GetRedirects(*urls_it_);
}

void ThreatDetailsRedirectsCollector::GetRedirects(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!history_service_) {
    AllDone();
    return;
  }

  history_service_->QueryRedirectsTo(
      url,
      base::BindOnce(&ThreatDetailsRedirectsCollector::OnGotQueryRedirectsTo,
                     weak_factory_.GetWeakPtr(), url),
      &request_tracker_);
}

void ThreatDetailsRedirectsCollector::OnGotQueryRedirectsTo(
    const GURL& url,
    history::RedirectList redirect_list) {
  if (!redirect_list.empty()) {
    std::vector<GURL> urllist;
    urllist.push_back(url);
    urllist.insert(urllist.end(), redirect_list.begin(), redirect_list.end());
    redirects_urls_.push_back(urllist);
  }

  // Proceed to next url
  ++urls_it_;

  if (urls_it_ == urls_.end()) {
    AllDone();
    return;
  }

  GetRedirects(*urls_it_);
}

void ThreatDetailsRedirectsCollector::AllDone() {
  DVLOG(1) << "AllDone";
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback_));
}

void ThreatDetailsRedirectsCollector::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_service_observation_.IsObservingSource(history_service));
  history_service_observation_.Reset();
  history_service_.reset();
}

}  // namespace safe_browsing
