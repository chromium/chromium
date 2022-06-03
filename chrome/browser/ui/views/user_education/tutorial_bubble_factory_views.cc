// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/tutorial_bubble_factory_views.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_owner_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

TutorialBubbleViews::TutorialBubbleViews(absl::optional<base::Token> bubble_id)
    : bubble_id_(bubble_id) {}

TutorialBubbleViews::~TutorialBubbleViews() {
  FeaturePromoBubbleOwnerImpl* owner_impl =
      FeaturePromoBubbleOwnerImpl::GetInstance();
  if (bubble_id_ && owner_impl->BubbleIsShowing(bubble_id_.value())) {
    owner_impl->CloseBubble(bubble_id_.value());
  }
}

TutorialBubbleFactoryViews::TutorialBubbleFactoryViews() = default;
TutorialBubbleFactoryViews::~TutorialBubbleFactoryViews() = default;

std::unique_ptr<TutorialBubble> TutorialBubbleFactoryViews::CreateBubble(
    ui::TrackedElement* element,
    absl::optional<std::u16string> title_text,
    absl::optional<std::u16string> body_text,
    TutorialDescription::Step::Arrow arrow,
    absl::optional<std::pair<int, int>> progress,
    base::RepeatingClosure abort_callback,
    bool is_last_step) {
  if (!element->IsA<views::TrackedElementViews>())
    return nullptr;

  views::View* view = element->AsA<views::TrackedElementViews>()->view();

  FeaturePromoBubbleOwnerImpl* owner_impl =
      FeaturePromoBubbleOwnerImpl::GetInstance();

  FeaturePromoBubbleView::CreateParams params;
  if (body_text) {
    params.body_text = body_text.value();
    params.screenreader_text = body_text.value();
  }
  params.anchor_view = view;
  if (title_text)
    params.title_text = title_text.value();
  if (progress) {
    params.tutorial_progress_current = progress->first;
    params.tutorial_progress_max = progress->second;
  }

  switch (arrow) {
    case TutorialDescription::Step::Arrow::TOP:
      params.arrow = views::BubbleBorder::Arrow::TOP_CENTER;
      break;
    case TutorialDescription::Step::Arrow::BOTTOM:
      params.arrow = views::BubbleBorder::Arrow::BOTTOM_CENTER;
      break;

    // TODO read RTL State to choose direction
    case TutorialDescription::Step::Arrow::TOP_HORIZONTAL:
      params.arrow = views::BubbleBorder::Arrow::LEFT_TOP;
      break;
    case TutorialDescription::Step::Arrow::CENTER_HORIZONTAL:
      params.arrow = views::BubbleBorder::Arrow::LEFT_CENTER;
      break;
    case TutorialDescription::Step::Arrow::BOTTOM_HORIZONTAL:
      params.arrow = views::BubbleBorder::Arrow::LEFT_BOTTOM;
      break;

    // in some cases we wont be able to place the arrow (Mac Menus)
    case TutorialDescription::Step::Arrow::NONE:
      params.arrow = views::BubbleBorder::Arrow::NONE;
      break;
  }

  // Set bubbles other than the final one to not time out; final bubble uses
  // default timeout.
  if (!is_last_step)
    params.timeout = base::TimeDelta();

  if (abort_callback) {
    params.has_close_button = true;
    params.dismiss_callback = abort_callback;
  }

  return std::make_unique<TutorialBubbleViews>(
      owner_impl->ShowBubble(std::move(params), base::BindOnce([] {})));
}

bool TutorialBubbleFactoryViews::CanBuildBubbleForTrackedElement(
    ui::TrackedElement* element) {
  return element->IsA<views::TrackedElementViews>();
}
