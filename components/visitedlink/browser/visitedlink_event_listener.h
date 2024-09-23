// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_EVENT_LISTENER_H_
#define COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_EVENT_LISTENER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host_observer.h"

namespace content {
class BrowserContext;
}

namespace visitedlink {

class VisitedLinkUpdater;

// VisitedLinkEventListener broadcasts link coloring database updates to all
// processes. It also coalesces the updates to avoid excessive broadcasting of
// messages to the renderers.
class VisitedLinkEventListener
    : public PartitionedVisitedLinkWriter::Listener,
      public VisitedLinkWriter::Listener,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver,
      public content::RenderWidgetHostObserver {
 public:
  explicit VisitedLinkEventListener(content::BrowserContext* browser_context);

  // Used by PartitionedVisitedLinkWriter to provide and store a raw pointer to
  // the owning object.
  VisitedLinkEventListener(content::BrowserContext* browser_context,
                           PartitionedVisitedLinkWriter* partitioned_writer);

  VisitedLinkEventListener(const VisitedLinkEventListener&) = delete;
  VisitedLinkEventListener& operator=(const VisitedLinkEventListener&) = delete;

  ~VisitedLinkEventListener() override;

  // (Partitioned)VisitedLinkWriter::Listener overrides.
  void NewTable(base::ReadOnlySharedMemoryRegion* table_region) override;
  void Add(VisitedLinkWriter::Fingerprint fingerprint) override;
  void Reset(bool invalidate_hashes) override;
  void UpdateOriginSalts() override;

  // Sets a custom timer to use for coalescing events for testing.
  // |coalesce_timer_override| must outlive this.
  void SetCoalesceTimerForTest(base::OneShotTimer* coalesce_timer_override);

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* rph) override;

  // content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* rwh,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(content::RenderWidgetHost* rwh) override;

 private:
  void CommitVisitedLinks();

  // The default Timer to use for coalescing events. This should not be used
  // directly to allow overriding it in tests. Instead, |coalesce_timer_|
  // should be used.
  base::OneShotTimer default_coalesce_timer_;
  // A pointer to either |default_coalesce_timer_| or to an override set using
  // SetCoalesceTimerForTest(). This does not own the timer.
  raw_ptr<base::OneShotTimer> coalesce_timer_;
  VisitedLinkCommon::Fingerprints pending_visited_links_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  base::ScopedMultiSourceObservation<content::RenderWidgetHost,
                                     content::RenderWidgetHostObserver>
      widget_observation_{this};

  // Map between renderer child ids and their VisitedLinkUpdater.
  std::map<int, std::unique_ptr<VisitedLinkUpdater>> updaters_;

  base::ReadOnlySharedMemoryRegion table_region_;

  // When constructed by a PartitionedVisitedLinkWriter, serves as pointer
  // to owner - client is responsible for keeping this pointer valid
  // during the lifetime of this VisitedLinkEventListener. Pointer is null if
  // constructed by an unpartitioned VisitedLinkWriter.
  raw_ptr<PartitionedVisitedLinkWriter> partitioned_writer_;

  // Used to filter RENDERER_PROCESS_CREATED notifications to renderers that
  // belong to this BrowserContext.
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_EVENT_LISTENER_H_
