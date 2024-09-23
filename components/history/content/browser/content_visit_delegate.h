// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_CONTENT_VISIT_DELEGATE_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_CONTENT_VISIT_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/core/visited_link.h"

namespace content {
class BrowserContext;
}

namespace url {
class Origin;
}

namespace visitedlink {
class VisitedLinkWriter;
}

using VisitedLink = visitedlink::VisitedLink;

namespace history {

// ContentVisitDelegate bridge VisitDelegate events to
// visitedlink::VisitedLinkWriter.
class ContentVisitDelegate : public VisitDelegate,
                             public visitedlink::VisitedLinkDelegate {
 public:
  explicit ContentVisitDelegate(content::BrowserContext* browser_context);

  ContentVisitDelegate(const ContentVisitDelegate&) = delete;
  ContentVisitDelegate& operator=(const ContentVisitDelegate&) = delete;

  ~ContentVisitDelegate() override;

 private:
  // Implementation of VisitDelegate.
  bool Init(HistoryService* history_service) override;
  void AddURL(const GURL& url) override;
  void AddURLs(const std::vector<GURL>& urls) override;
  void DeleteURLs(const std::vector<GURL>& urls) override;
  void DeleteAllURLs() override;
  void AddVisitedLink(const VisitedLink& link) override;
  void DeleteVisitedLinks(const std::vector<VisitedLink>& links) override;
  void DeleteAllVisitedLinks() override;
  std::optional<uint64_t> GetOrAddOriginSalt(
      const url::Origin& origin) override;

  // Implementation of visitedlink::VisitedLinkDelegate.
  void RebuildTable(const scoped_refptr<
      visitedlink::VisitedLinkDelegate::URLEnumerator>& enumerator) override;
  void BuildVisitedLinkTable(
      const scoped_refptr<
          visitedlink::VisitedLinkDelegate::VisitedLinkEnumerator>& enumerator)
      override;

  raw_ptr<HistoryService> history_service_;

  // Visited Links -----------------------------------------------------------
  // To keep the partitioned visited links experiments performant, we want to
  // ensure that only one writer (partitioned or unpartitioned) is constructed
  // and initialized at a time.
  //
  // If neither `kPartitionVisitedLinkDatabase` nor
  // `kPartitionVisitedLinkDatabaseWithSelfLinks` is enabled,
  // `visitedlink_writer_` is constructed and initialized, while
  // `partitioned_writer_` is nullopt. This state is referred to as
  // "unpartitioned".
  //
  // If either `kPartitionVisitedLinkDatabase` or
  // `kPartitionVisitedLinkDatabaseWithSelfLinks` is enabled,
  // `visitedlink_writer_` is nullopt, while `partitioned_writer_` is
  // constructed and initialized. This state is referred to as "partitioned".
  std::unique_ptr<visitedlink::PartitionedVisitedLinkWriter>
      partitioned_writer_;
  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;
  // -------------------------------------------------------------------------

  base::CancelableTaskTracker task_tracker_;
  base::WeakPtrFactory<ContentVisitDelegate> weak_factory_{this};
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_CONTENT_VISIT_DELEGATE_H_
