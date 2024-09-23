// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DELEGATE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DELEGATE_H_

#include <cstdint>
#include <optional>
#include <vector>

class GURL;

namespace url {
class Origin;
}

namespace visitedlink {
struct VisitedLink;
}

using VisitedLink = visitedlink::VisitedLink;

namespace history {

class HistoryService;

// VisitDelegate gets notified about URLs recorded as visited by the
// HistoryService. NOTE: Add and Delete VisitedLink mutations (not including
// GetOrAddOriginSalt()) MUST be called after going to the history DB thread.
// Otherwise, multiple mutation calls risk being applied out of order.
class VisitDelegate {
 public:
  VisitDelegate();

  VisitDelegate(const VisitDelegate&) = delete;
  VisitDelegate& operator=(const VisitDelegate&) = delete;

  virtual ~VisitDelegate();

  // Called once HistoryService initialization is complete. Returns true if the
  // initialization succeeded, false otherwise.
  virtual bool Init(HistoryService* history_service) = 0;

  // Called when an URL is recorded by HistoryService.
  virtual void AddURL(const GURL& url) = 0;

  // Called when a list of URLs are recorded by HistoryService.
  virtual void AddURLs(const std::vector<GURL>& urls) = 0;

  // Called when a list of URLs are removed from HistoryService.
  virtual void DeleteURLs(const std::vector<GURL>& urls) = 0;

  // Called when all URLs are removed from HistoryService.
  virtual void DeleteAllURLs() = 0;

  // Called when a partitioned visited link is recorded by HistoryService.
  virtual void AddVisitedLink(const VisitedLink& link) = 0;

  // Called when a list of partitioned visited links is removed from
  // HistoryService.
  virtual void DeleteVisitedLinks(const std::vector<VisitedLink>& links) = 0;

  // Called when all partitioned visited links are removed from HistoryService.
  virtual void DeleteAllVisitedLinks() = 0;

  // Returns the hash salt corresponding to the given origin. If we have not
  // previously navigated to `origin`, a new <origin, salt> pair will be
  // generated, and that new salt value will be returned.
  // NOTE: the <origin, salt> map is never pruned, so it is safe to call this
  // function at any time without risk of add/delete race conditions.
  virtual std::optional<uint64_t> GetOrAddOriginSalt(
      const url::Origin& origin) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DELEGATE_H_
