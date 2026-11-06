// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRERENDERER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRERENDERER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Holds one spare WebUI WebContents for the next consumer to take, and builds
// a new one after each take.
//
// After a take, the refill waits for the taken WebContents to complete its
// first visually non-empty paint (or immediately if already painted), or for
// the paint deadline, and then for a delay, so that the build stays off a
// renderer that may be shared with the taken WebContents while it renders.
// Destroying the watched WebContents before it paints does not cancel the
// refill: the prerenderer waits for the rest of the paint deadline, as if no
// WebContents were watched.
//
// The refill runs on the UI thread at best-effort priority, yielding to input
// and startup tasks. If the memory limit is at or below
// `Params::skip_memory_limit` when the delay elapses, the refill is skipped,
// to avoid competing for system resources. The delegate builds each spare and
// decides whether what a spare read when it navigated is still current.
class WebUIContentsPrerenderer : public content::WebContentsObserver,
                                 public base::PassiveMemoryConsumer {
 public:
  struct Params {
    // Upper bound on how long a refill waits for the taken contents to paint
    // before starting `delay` anyway.
    base::TimeDelta paint_deadline;
    // How long to wait after the paint or the paint deadline.
    base::TimeDelta delay;
    // The memory limit at or below which the refill is skipped.
    base::MemoryLimit skip_memory_limit;
    // The name under which the prerenderer registers as a memory consumer.
    std::string_view consumer_name;
  };

  // The outcome of `Take()`. These values are persisted to logs. Entries should
  // not be renumbered and numeric values should never be reused.
  //
  // LINT.IfChange(WebUIContentsPrerenderTakeResult)
  enum class TakeResult {
    // The consumer took the spare.
    kAdopted = 0,
    // There was no spare to take.
    kNoSpare = 1,
    // The spare was dropped because what it read when it navigated has changed
    // since.
    kDroppedStale = 2,
    // The spare was dropped because its renderer is gone.
    kDroppedCrashed = 3,
    // The spare was dropped because it already completed its first visually
    // non-empty paint. First-paint notifications are one-shot, so adopting it
    // would leave the consumer's surface-sync deadline uncleared.
    kDroppedAlreadyPainted = 4,
    kMaxValue = kDroppedAlreadyPainted,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:WebUIContentsPrerenderTakeResult)

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Builds a new spare. Returns null to skip this refill.
    virtual std::unique_ptr<content::WebContents> CreateSpare() = 0;

    // Returns whether what `spare` read when it navigated is still current.
    // Only called for a spare whose renderer still runs and that has not
    // painted yet.
    virtual bool IsSpareCurrent(content::WebContents& spare) = 0;
  };

  struct TakeOutcome {
    // The spare, or null if there was none or it was dropped.
    std::unique_ptr<content::WebContents> contents;
    TakeResult result;
  };

  // `delegate` must outlive this object.
  WebUIContentsPrerenderer(Delegate& delegate, const Params& params);
  WebUIContentsPrerenderer(const WebUIContentsPrerenderer&) = delete;
  WebUIContentsPrerenderer& operator=(const WebUIContentsPrerenderer&) = delete;
  ~WebUIContentsPrerenderer() override;

  // Takes the spare if it can be adopted. A dropped spare is destroyed
  // synchronously. Does not schedule a refill.
  TakeOutcome Take();

  // Builds the next spare after `taken_contents` paints. A null
  // `taken_contents` (the consumer built its own WebContents) waits for the
  // paint deadline only. Replaces any refill that is already pending.
  void ScheduleRefill(content::WebContents* taken_contents);

  // Drops the spare and any pending refill.
  void Reset();

  // Returns whether a spare is waiting to be taken. It may still be dropped
  // when taken.
  bool has_spare() const { return spare_ != nullptr; }

  content::WebContents* spare_for_testing() { return spare_.get(); }
  bool is_refill_pending_for_testing() const { return timer_.IsRunning(); }
  // Overrides the paint deadline for later `ScheduleRefill()` calls.
  void set_paint_deadline_for_testing(base::TimeDelta deadline) {
    params_.paint_deadline = deadline;
  }

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;

 private:
  void StartDelay();
  void Refill();
  void CancelRefill();

  const raw_ref<Delegate> delegate_;
  Params params_;
  std::unique_ptr<content::WebContents> spare_;
  base::OneShotTimer timer_;
  std::optional<base::MemoryConsumerRegistration> memory_consumer_registration_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_PRERENDERER_H_
