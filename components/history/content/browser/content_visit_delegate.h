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
#include "components/visitedlink/browser/visitedlink_delegate.h"

namespace content {
class BrowserContext;
}

namespace visitedlink {
class VisitedLinkWriter;
}

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

  // Implementation of visitedlink::VisitedLinkDelegate.
  void RebuildTable(const scoped_refptr<
      visitedlink::VisitedLinkDelegate::URLEnumerator>& enumerator) override;

  raw_ptr<HistoryService> history_service_;  // Weak.
  std::unique_ptr<visitedlink::VisitedLinkWriter> visitedlink_writer_;
  base::CancelableTaskTracker task_tracker_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_CONTENT_VISIT_DELEGATE_H_
