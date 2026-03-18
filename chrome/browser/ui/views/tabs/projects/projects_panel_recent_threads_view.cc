// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"

#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/layout/box_layout.h"

namespace {
constexpr base::TimeDelta kExpandAnimationTime = base::Milliseconds(250);

static bool disable_animations_for_testing_ = false;
}  // namespace

ProjectsPanelRecentThreadsView::ProjectsPanelRecentThreadsView(
    ThreadPressedCallback thread_button_callback)
    : thread_button_callback_(std::move(thread_button_callback)) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_->SetOrientation(views::BoxLayout::Orientation::kVertical);

  expansion_animation_.SetTweenType(gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED);
}

ProjectsPanelRecentThreadsView::~ProjectsPanelRecentThreadsView() = default;

void ProjectsPanelRecentThreadsView::SetThreads(
    const std::vector<contextual_tasks::Thread>& threads) {
  threads_ = threads;
  UpdateDisplayedThreads();
}

void ProjectsPanelRecentThreadsView::SetExpanded(bool expanded) {
  if (expanded_ == expanded) {
    return;
  }

  // Compute the start and end heights after changing the number of threads
  // displayed.
  start_height_ = GetPreferredSize().height();
  expanded_ = expanded;
  UpdateDisplayedThreads();
  target_height_ = GetPreferredSize().height();

  if (disable_animations_for_testing_) {
    PreferredSizeChanged();
    return;
  }

  expansion_animation_.Reset(0.0);
  expansion_animation_.SetSlideDuration(kExpandAnimationTime);
  expansion_animation_.Show();
}

void ProjectsPanelRecentThreadsView::SetInsideBorderInsets(
    const gfx::Insets& insets) {
  layout_->set_inside_border_insets(insets);
}

void ProjectsPanelRecentThreadsView::UpdateDisplayedThreads() {
  item_views_.clear();
  RemoveAllChildViews();

  size_t count = 0;
  for (const auto& thread : threads_) {
    if (count >= projects_panel::kMaxNumberOfRecentThreads ||
        (!expanded_ &&
         count >= projects_panel::kNumThreadsVisibleWhenCollapsed)) {
      break;
    }
    auto* item_view =
        AddChildView(std::make_unique<ProjectsPanelThreadItemView>(
            thread, thread_button_callback_));
    if (disable_animations_for_testing_) {
      item_view->disable_animations_for_testing();  // IN-TEST
    }
    item_views_.push_back(item_view);
    count++;
  }
}

gfx::Size ProjectsPanelRecentThreadsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  if (expansion_animation_.is_animating()) {
    size.set_height(gfx::Tween::IntValueBetween(
        expansion_animation_.GetCurrentValue(), start_height_, target_height_));
  }
  return size;
}

void ProjectsPanelRecentThreadsView::AnimationProgressed(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

void ProjectsPanelRecentThreadsView::AnimationEnded(
    const gfx::Animation* animation) {
  PreferredSizeChanged();
}

// static
void ProjectsPanelRecentThreadsView::disable_animations_for_testing() {
  disable_animations_for_testing_ = true;
}

BEGIN_METADATA(ProjectsPanelRecentThreadsView)
END_METADATA
