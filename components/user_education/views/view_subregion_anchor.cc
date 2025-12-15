// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/view_subregion_anchor.h"

#include <algorithm>

#include "components/user_education/common/user_education_events.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace user_education {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(ViewSubregionAnchor)

ViewSubregionAnchor::ViewSubregionAnchor(ui::ElementIdentifier id,
                                         views::View& view)
    : TrackedElement(id, views::ElementTrackerViews::GetContextForView(&view)),
      view_(view) {
  const ui::ElementIdentifier view_id =
      view.GetProperty(views::kElementIdentifierKey);
  CHECK(view_id);
  anchor_view_shown_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          view_id, context(),
          base::BindRepeating(&ViewSubregionAnchor::OnAnchorViewShown,
                              base::Unretained(this)));
  anchor_view_hidden_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          view_id, context(),
          base::BindRepeating(&ViewSubregionAnchor::OnAnchorViewHidden,
                              base::Unretained(this)));
  auto candidates =
      ui::ElementTracker::GetElementTracker()->GetAllMatchingElements(
          view_id, context());
  view_visible_ = std::ranges::any_of(
      candidates, [to_find = &view](ui::TrackedElement* found) {
        auto* const view_el = found->AsA<views::TrackedElementViews>();
        return view_el && view_el->view() == to_find;
      });
  UpdateVisibility();
}

ViewSubregionAnchor::~ViewSubregionAnchor() {
  manually_hidden_ = true;
  UpdateVisibility();
}

void ViewSubregionAnchor::MaybeUpdateAnchor(gfx::Rect local_anchor_region) {
  if (last_anchor_region_ == local_anchor_region) {
    return;
  }

  last_anchor_region_ = local_anchor_region;
  if (visible_) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        this, kHelpBubbleAnchorBoundsChangedEvent);
  }
}

gfx::Rect ViewSubregionAnchor::GetScreenBounds() const {
  gfx::Rect screen_bounds = last_anchor_region_;
  views::View::ConvertRectToScreen(&*view_, &screen_bounds);
  return screen_bounds;
}

gfx::NativeView ViewSubregionAnchor::GetNativeView() const {
  if (!view_->GetWidget()) {
    return gfx::NativeView();
  }
  return view_->GetWidget()->GetNativeView();
}

std::string ViewSubregionAnchor::ToString() const {
  auto result = TrackedElement::ToString();
  result.append(" tracking view ");
  result.append(view_->GetClassName());
  return result;
}

void ViewSubregionAnchor::SetHidden(bool hidden) {
  if (manually_hidden_ == hidden) {
    return;
  }

  manually_hidden_ = hidden;
  UpdateVisibility();
}

void ViewSubregionAnchor::OnAnchorViewShown(ui::TrackedElement* el) {
  if (auto* const view_el = el->AsA<views::TrackedElementViews>()) {
    if (view_el->view() == &*view_) {
      view_visible_ = true;
      UpdateVisibility();
    }
  }
}

void ViewSubregionAnchor::OnAnchorViewHidden(ui::TrackedElement* el) {
  if (auto* const view_el = el->AsA<views::TrackedElementViews>()) {
    if (view_el->view() == &*view_) {
      view_visible_ = false;
      UpdateVisibility();
    }
  }
}

void ViewSubregionAnchor::UpdateVisibility() {
  const bool is_visible = view_visible_ && !manually_hidden_;
  if (is_visible == visible_) {
    return;
  }

  visible_ = is_visible;
  if (visible_) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
  } else {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(this);
  }
}

}  // namespace user_education
