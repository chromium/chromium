// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_EVENT_LISTENER_H_
#define COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_EVENT_LISTENER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/timer/timer.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class BrowserContext;
}

namespace visitedlink {

class VisitedLinkUpdater;

// VisitedLinkEventListener broadcasts link coloring database updates to all
// processes. It also coalesces the updates to avoid excessive broadcasting of
// messages to the renderers.
class VisitedLinkEventListener : public VisitedLinkWriter::Listener,
                                 public content::NotificationObserver {
 public:
  explicit VisitedLinkEventListener(content::BrowserContext* browser_context);
  ~VisitedLinkEventListener() override;

  void NewTable(base::ReadOnlySharedMemoryRegion* table_region) override;
  void Add(VisitedLinkWriter::Fingerprint fingerprint) override;
  void Reset(bool invalidate_hashes) override;

  // Sets a custom timer to use for coalescing events for testing.
  // |coalesce_timer_override| must outlive this.
  void SetCoalesceTimerForTest(base::OneShotTimer* coalesce_timer_override);

 private:
  void CommitVisitedLinks();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The default Timer to use for coalescing events. This should not be used
  // directly to allow overriding it in tests. Instead, |coalesce_timer_|
  // should be used.
  base::OneShotTimer default_coalesce_timer_;
  // A pointer to either |default_coalesce_timer_| or to an override set using
  // SetCoalesceTimerForTest(). This does not own the timer.
  base::OneShotTimer* coalesce_timer_;
  VisitedLinkCommon::Fingerprints pending_visited_links_;

  content::NotificationRegistrar registrar_;

  // Map between renderer child ids and their VisitedLinkUpdater.
  typedef std::map<int, std::unique_ptr<VisitedLinkUpdater>> Updaters;
  Updaters updaters_;

  base::ReadOnlySharedMemoryRegion table_region_;

  // Used to filter RENDERER_PROCESS_CREATED notifications to renderers that
  // belong to this BrowserContext.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(VisitedLinkEventListener);
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_EVENT_LISTENER_H_
