// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/visited_link_navigation_throttle.h"

#include "components/history/core/browser/history_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/origin.h"

VisitedLinkNavigationThrottle::VisitedLinkNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    history::HistoryService* history_service)
    : content::NavigationThrottle(navigation_handle),
      history_service_(history_service) {}

VisitedLinkNavigationThrottle::~VisitedLinkNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
VisitedLinkNavigationThrottle::WillProcessResponse() {
  // If we cannot obtain the origin to be committed, we cannot determine the
  // salt. The commit params are constructed with an std::nullopt value for salt
  // by default. This should only occur in cases that do not actually commit a
  // navigation (e.g. downloads), so sending a salt value is not necessary.
  std::optional<url::Origin> origin_to_commit =
      navigation_handle()->GetOriginToCommit();
  if (!origin_to_commit.has_value()) {
    return PROCEED;
  }
  // Obtain the origin's corresponding salt and assign it in `commit_params`. If
  // this origin is not already in the VisitedLinkDatabase, we will add a new
  // <origin, salt> pair to the map, and assign that new salt value to
  // 'commit_params`. If the visited link hashtable is in the middle of being
  // built on the DB thread at the time of our request, `salt` will be set to
  // std::nullopt. In this case, VisitedLinkWriter will handle sending the
  // correct salt value after hashtable initialization is complete.
  const std::optional<uint64_t> salt =
      history_service_->GetOrAddOriginSalt(origin_to_commit.value());
  if (salt.has_value()) {
    navigation_handle()->SetVisitedLinkSalt(salt.value());
  }
  return PROCEED;
}

const char* VisitedLinkNavigationThrottle::GetNameForLogging() {
  return "VisitedLinkNavigationThrottle";
}

void VisitedLinkNavigationThrottle::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  // In the unexpected event of the HistoryService being deleted, this CHECK
  // helps us avoid dereferencing the now null raw_pointer, and helps us
  // identify potential lifetime issues.
  CHECK(false)
      << "The HistoryService owned by VisitedLinkNavigationThrottle is "
         "being deleted";
}
