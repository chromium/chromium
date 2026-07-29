// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACTOR_CHROME_PAGE_STABILITY_MONITOR_DELEGATE_H_
#define CHROME_RENDERER_ACTOR_CHROME_PAGE_STABILITY_MONITOR_DELEGATE_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "chrome/renderer/actor/journal.h"
#include "components/actor/core/page_stability_monitor_delegate.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

// Chrome-specific implementation of the PageStabilityMonitorDelegate that
// routes page stability events to the Actor Journal.
class ChromePageStabilityMonitorDelegate : public PageStabilityMonitorDelegate {
 public:
  ChromePageStabilityMonitorDelegate(TaskId task_id,
                                     Journal& journal,
                                     const Thresholds& thresholds);
  ~ChromePageStabilityMonitorDelegate() override;

 protected:
  // PageStabilityMonitorDelegate:
  void LogEvent(mojom::JournalEntryType type,
                std::string_view event_name,
                std::vector<mojom::JournalDetailsPtr> details) override;

 private:
  // The Journal to log events to.
  base::raw_ref<Journal> journal_;

  // The pending entry for the currently active page stability state, if any.
  std::unique_ptr<Journal::PendingAsyncEntry> async_journal_entry_;
};

}  // namespace actor

#endif  // CHROME_RENDERER_ACTOR_CHROME_PAGE_STABILITY_MONITOR_DELEGATE_H_
