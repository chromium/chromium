// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_container_with_priority.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/infobars/core/features.h"
#include "components/infobars/core/infobar.h"

namespace infobars {

namespace {
// Returns the visible cap for a given priority.
size_t GetInfoBarPriorityCapFor(InfoBarDelegate::InfobarPriority priority) {
  std::optional<InfobarPriorityCaps> caps = GetInfobarPriorityCaps();
  CHECK(caps.has_value());
  switch (priority) {
    case InfoBarDelegate::InfobarPriority::kCriticalSecurity:
      return caps->max_visible_critical;
    case InfoBarDelegate::InfobarPriority::kDefault:
      return caps->max_visible_default;
    case InfoBarDelegate::InfobarPriority::kLow:
      return caps->max_visible_low;
  }

  NOTREACHED();
}

// Helper to get histogram suffix based on priority.
std::string GetPrioritySuffix(InfoBarDelegate::InfobarPriority priority) {
  switch (priority) {
    case InfoBarDelegate::InfobarPriority::kCriticalSecurity:
      return "CriticalSecurity";
    case InfoBarDelegate::InfobarPriority::kDefault:
      return "Default";
    case InfoBarDelegate::InfobarPriority::kLow:
      return "Low";
  }

  NOTREACHED();
}

}  // namespace

InfoBarContainerWithPriority::InfoBarContainerWithPriority(Delegate* delegate)
    : InfoBarContainer(delegate) {}

InfoBarContainerWithPriority::~InfoBarContainerWithPriority() = default;

void InfoBarContainerWithPriority::ChangeInfoBarManager(
    InfoBarManager* infobar_manager) {
  if (!IsInfobarPrioritizationEnabled()) {
    InfoBarContainer::ChangeInfoBarManager(infobar_manager);
    return;
  }

  scoped_observation_.Reset();

  bool state_changed = !infobars().empty();
  {
    // Use the base class's mechanism to suppress all intermediate calls to
    // OnInfoBarStateChanged. This ensures we only re-layout the UI once.
    base::AutoReset<bool> ignore_changes(&ignore_infobar_state_changed_, true);

    // Hide and remove all existing infobars without animation. Hiding the
    // infobar triggers its removal from the container.
    while (!infobars().empty()) {
      infobars().front()->Hide(false);
    }

    // If there are items left in the queue when the manager changes/shuts down,
    // they are considered "starved".
    if (!pending_infobars_.empty()) {
      base::UmaHistogramCounts100("InfoBar.Prioritization.StarvedCount",
                                  pending_infobars_.size());
    }

    pending_infobars_.clear();
    visible_.clear();

    if (infobar_manager) {
      scoped_observation_.Observe(infobar_manager);
      if (!manager()->infobars().empty()) {
        state_changed = true;
      }

      // OnInfoBarAdded contains the AdmitOrQueue logic needed to properly
      // handle prioritization.
      for (InfoBar* infobar : manager()->infobars()) {
        OnInfoBarAdded(infobar);
      }
    }
  }

  if (state_changed) {
    OnInfoBarStateChanged(false);
  }
}

void InfoBarContainerWithPriority::OnInfoBarAdded(InfoBar* infobar) {
  if (!IsInfobarPrioritizationEnabled()) {
    InfoBarContainer::OnInfoBarAdded(infobar);
    return;
  }

  const auto priority = infobar->delegate()
                            ? infobar->delegate()->GetPriority()
                            : InfoBarDelegate::InfobarPriority::kDefault;
  AdmitOrQueue(infobar, priority);
}

void InfoBarContainerWithPriority::OnInfoBarRemoved(InfoBar* infobar,
                                                    bool animate) {
  if (!IsInfobarPrioritizationEnabled()) {
    InfoBarContainer::OnInfoBarRemoved(infobar, animate);
    return;
  }

  // An infobar is being removed from the manager. It could be in our visible
  // list or our pending list. We must remove it from whichever list it's in
  // to prevent holding a dangling pointer.
  //
  // 1. Try to remove from pending.
  size_t pending_removed = std::erase_if(
      pending_infobars_, [infobar](const PendingInfoBarEntry& entry) {
        return entry.infobar == infobar;
      });

  // 2. Try to remove from visible.
  size_t visible_removed = ClearVisible(infobar);

  if (visible_removed > 0) {
    // It was visible, so it is in the base container.
    // We must call base::OnInfoBarRemoved to clear owner and remove from view.
    InfoBarContainer::OnInfoBarRemoved(infobar, animate);

    // Now that a slot is free, try to promote from pending.
    Promote();
  } else {
    // It was not visible. It must have been pending. Do NOT call
    // base::OnInfoBarRemoved because it was never added to the base container.
    CHECK(pending_removed > 0 || !infobar->delegate());
  }
}

void InfoBarContainerWithPriority::OnInfoBarReplaced(InfoBar* old_infobar,
                                                     InfoBar* new_infobar) {
  if (!IsInfobarPrioritizationEnabled()) {
    InfoBarContainer::OnInfoBarReplaced(old_infobar, new_infobar);
    return;
  }

  // Track whether the old infobar was actually visible in this container.
  const bool was_visible =
      std::ranges::any_of(visible_, [old_infobar](const VisibleEntry& entry) {
        return entry.infobar == old_infobar;
      });

  // Remove any tracking for the old infobar from our visibility buckets.
  ClearVisible(old_infobar);

  // Let the base class update its own internal state (infobars_ list and
  // platform-specific views). This keeps the usual lifetime and container
  // invariants intact and avoids double-calling CloseSoon().
  InfoBarContainer::OnInfoBarReplaced(old_infobar, new_infobar);

  // If the old infobar was visible, the replacement should be tracked as
  // visible as well, under its priority.
  if (was_visible) {
    const auto new_priority = new_infobar->delegate()
                                  ? new_infobar->delegate()->GetPriority()
                                  : InfoBarDelegate::InfobarPriority::kDefault;
    MarkVisible(new_infobar, new_priority);
  }
}

void InfoBarContainerWithPriority::AddInfoBarAndTrack(
    InfoBar* infobar,
    size_t position,
    bool animate,
    InfoBarDelegate::InfobarPriority priority) {
  AddInfoBar(infobar, position, animate);
  MarkVisible(infobar, priority);
  RecordInfoBarPendingQueueSize();
}

void InfoBarContainerWithPriority::AdmitOrQueue(
    InfoBar* infobar,
    InfoBarDelegate::InfobarPriority priority) {
  // If an identical infobar is already in the queue, do nothing. This avoids
  // enqueuing duplicates.
  if (IsDuplicateOfPending(infobar)) {
    return;
  }

  // 1) Handle CRITICAL priority.
  if (priority == InfoBarDelegate::InfobarPriority::kCriticalSecurity) {
    if (CountVisible(priority) < GetInfoBarPriorityCapFor(priority)) {
      AddInfoBarAndTrack(infobar, infobars().size(), /*animate=*/true,
                         priority);
    } else {
      EnqueueInfoBar(infobar, priority);
    }
    return;
  }

  // From this point on, only DEFAULT and LOW priorities are handled.
  // Lower priorities are queued if any CRITICAL-priority infobar is visible
  // or pending.
  if (CountVisible(InfoBarDelegate::InfobarPriority::kCriticalSecurity) > 0 ||
      HasPendingOfPriority(
          InfoBarDelegate::InfobarPriority::kCriticalSecurity)) {
    EnqueueInfoBar(infobar, priority);
    return;
  }

  // 2) Handle DEFAULT priority.
  if (priority == InfoBarDelegate::InfobarPriority::kDefault) {
    // A DEFAULT-priority infobar is only shown if its cap has not been
    // reached and it would not preempt an already-visible LOW-priority one.
    if (CountVisible(InfoBarDelegate::InfobarPriority::kLow) == 0 &&
        CountVisible(priority) < GetInfoBarPriorityCapFor(priority)) {
      AddInfoBarAndTrack(infobar, infobars().size(), /*animate=*/true,
                         priority);
    } else {
      EnqueueInfoBar(infobar, priority);
    }
    return;
  }

  // 3) Handle LOW priority.
  //
  // A LOW-priority infobar is only shown if no higher-priority infobars are
  // visible or pending, and if its own cap has not been reached.
  if (CountVisible(InfoBarDelegate::InfobarPriority::kDefault) == 0 &&
      !HasPendingOfPriority(InfoBarDelegate::InfobarPriority::kDefault) &&
      CountVisible(priority) < GetInfoBarPriorityCapFor(priority)) {
    AddInfoBarAndTrack(infobar, infobars().size(), /*animate=*/true, priority);
  } else {
    EnqueueInfoBar(infobar, priority);
  }
}

void InfoBarContainerWithPriority::Promote() {
  // 1) Promote CRITICAL-priority infobars.
  PromoteInfobarsOfPriority(
      InfoBarDelegate::InfobarPriority::kCriticalSecurity,
      GetInfoBarPriorityCapFor(
          InfoBarDelegate::InfobarPriority::kCriticalSecurity));

  // Defer LOW-priority promotion if any DEFAULT-priority infobar is visible or
  // pending.
  if (CountVisible(InfoBarDelegate::InfobarPriority::kCriticalSecurity) > 0 ||
      HasPendingOfPriority(
          InfoBarDelegate::InfobarPriority::kCriticalSecurity)) {
    return;
  }

  // 2) Promote DEFAULT-priority infobars.
  PromoteInfobarsOfPriority(
      InfoBarDelegate::InfobarPriority::kDefault,
      GetInfoBarPriorityCapFor(InfoBarDelegate::InfobarPriority::kDefault));

  // Defer LOW-priority promotion if any DEFAULT-priority infobar is visible or
  // pending.
  if (CountVisible(InfoBarDelegate::InfobarPriority::kDefault) > 0 ||
      HasPendingOfPriority(InfoBarDelegate::InfobarPriority::kDefault)) {
    return;
  }

  // 3) Promote LOW-priority infobars.
  PromoteInfobarsOfPriority(
      InfoBarDelegate::InfobarPriority::kLow,
      GetInfoBarPriorityCapFor(InfoBarDelegate::InfobarPriority::kLow));
}

void InfoBarContainerWithPriority::PromoteInfobarsOfPriority(
    InfoBarDelegate::InfobarPriority priority,
    size_t priority_cap) {
  while (CountVisible(priority) < priority_cap) {
    auto it = std::ranges::find(pending_infobars_, priority,
                                &PendingInfoBarEntry::priority);
    if (it == pending_infobars_.end()) {
      break;
    }

    const base::TimeDelta wait_time = base::TimeTicks::Now() - it->enqueued_at;
    base::UmaHistogramMediumTimes(
        base::StrCat({"InfoBar.Prioritization.", GetPrioritySuffix(priority),
                      "WaitTime"}),
        wait_time);

    InfoBar* bar_to_promote = it->infobar;
    pending_infobars_.erase(it);
    AddInfoBarAndTrack(bar_to_promote, infobars().size(), /*animate=*/true,
                       priority);
  }
}

void InfoBarContainerWithPriority::EnqueueInfoBar(
    InfoBar* infobar,
    InfoBarDelegate::InfobarPriority priority) {
  // Find the first entry with a lower priority.
  auto it = std::ranges::find_if(
      pending_infobars_,
      [priority](const auto& other) { return other.priority < priority; });

  // Insert the new entry before it. This maintains a descending priority
  // order. For entries of the same priority, this preserves FIFO order
  // because new entries are added to the end of their priority group.
  pending_infobars_.insert(it, {.infobar = infobar,
                                .priority = priority,
                                .enqueued_at = base::TimeTicks::Now()});
}

bool InfoBarContainerWithPriority::IsDuplicateOfPending(
    InfoBar* infobar) const {
  if (!infobar || !infobar->delegate()) {
    return false;
  }
  return std::ranges::any_of(pending_infobars_, [&](const auto& pending) {
    return pending.infobar && pending.infobar->delegate() &&
           pending.infobar->delegate()->EqualsDelegate(infobar->delegate());
  });
}

void InfoBarContainerWithPriority::MarkVisible(
    InfoBar* infobar,
    InfoBarDelegate::InfobarPriority priority) {
  visible_.push_back({.infobar = infobar, .priority = priority});
}

size_t InfoBarContainerWithPriority::ClearVisible(InfoBar* infobar) {
  return std::erase_if(visible_, [infobar](const VisibleEntry& entry) {
    return entry.infobar == infobar;
  });
}

size_t InfoBarContainerWithPriority::CountVisible(
    InfoBarDelegate::InfobarPriority priority) const {
  return std::ranges::count(visible_, priority, &VisibleEntry::priority);
}

bool InfoBarContainerWithPriority::HasPendingOfPriority(
    InfoBarDelegate::InfobarPriority priority) const {
  return std::ranges::any_of(pending_infobars_, [priority](const auto& entry) {
    return entry.priority == priority;
  });
}

void InfoBarContainerWithPriority::RecordInfoBarPendingQueueSize() {
  base::UmaHistogramExactLinear("InfoBar.Prioritization.QueueSize",
                                static_cast<int>(pending_infobars_.size()), 50);
}

}  // namespace infobars
