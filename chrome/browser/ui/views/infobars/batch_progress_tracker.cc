// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/batch_progress_tracker.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"

namespace {
constexpr char kBatchProgressTrackerKey = 0;
}  // namespace

// static
bool BatchProgressTracker::IsEligibleInfoBar(const infobars::InfoBar* infobar) {
  if (!infobar) {
    return false;
  }

  if (!infobar->delegate()) {
    return true;
  }

  // Critical-security and non-closeable infobars never host or advance the
  // sequential batch indicator.
  return infobar->delegate()->GetPriority() !=
             infobars::InfoBarDelegate::InfobarPriority::kCriticalSecurity &&
         infobar->delegate()->IsCloseable();
}

// static
BatchProgressTracker& BatchProgressTracker::GetOrCreate(
    infobars::InfoBarManager& manager) {
  if (auto* existing = Get(&manager)) {
    return *existing;
  }

  auto tracker = std::make_unique<BatchProgressTracker>(
      base::PassKey<BatchProgressTracker>(), manager);
  auto& ref = *tracker;
  manager.SetUserData(&kBatchProgressTrackerKey, std::move(tracker));
  return ref;
}

// static
BatchProgressTracker* BatchProgressTracker::Get(
    const infobars::InfoBarManager* manager) {
  if (!manager) {
    return nullptr;
  }

  return static_cast<BatchProgressTracker*>(
      manager->GetUserData(&kBatchProgressTrackerKey));
}

BatchProgressTracker::BatchProgressTracker(base::PassKey<BatchProgressTracker>,
                                           infobars::InfoBarManager& manager)
    : manager_(&manager) {
  observation_.Observe(&manager);
}

BatchProgressTracker::~BatchProgressTracker() = default;

BatchProgressTracker::Position BatchProgressTracker::ResolvePosition(
    const infobars::InfoBar* host,
    bool has_staying_eligible_bar,
    size_t pending_count) {
  CHECK(!active_infobar_ || !manager_ ||
        std::ranges::contains(manager_->infobars(), active_infobar_));

  if (!has_staying_eligible_bar && pending_count == 0) {
    Reset();
    return {};
  }

  if (host && pending_count > 0) {
    active_ = true;
  }

  if (!active_ || !host) {
    return {};
  }

  const size_t total = completed_count_ + 1 + pending_count;
  if (total <= 1) {
    Reset();
    return {};
  }

  active_infobar_ = host;
  return {.index = completed_count_ + 1, .total = total};
}

void BatchProgressTracker::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  if (!active_ || !manager_) {
    active_infobar_ = nullptr;
    return;
  }

  if (infobar == active_infobar_) {
    active_infobar_ = nullptr;
    ++completed_count_;
  }

  ResetIfBatchEnded();
}

void BatchProgressTracker::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                             infobars::InfoBar* new_infobar) {
  if (!active_ || !manager_) {
    active_infobar_ = nullptr;
    return;
  }

  if (old_infobar == active_infobar_) {
    active_infobar_ = IsEligibleInfoBar(new_infobar) ? new_infobar : nullptr;
  }

  ResetIfBatchEnded();
}

void BatchProgressTracker::ResetIfBatchEnded() {
  const size_t remaining_eligible = std::ranges::count_if(
      manager_->infobars(), &BatchProgressTracker::IsEligibleInfoBar);
  if (remaining_eligible == 0 ||
      (completed_count_ == 0 && remaining_eligible <= 1)) {
    Reset();
  }
}

void BatchProgressTracker::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  Reset();
  observation_.Reset();
  manager_ = nullptr;
}

void BatchProgressTracker::Reset() {
  active_infobar_ = nullptr;
  completed_count_ = 0;
  active_ = false;
}
