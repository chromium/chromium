// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_VIEW_SUBREGION_ANCHOR_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_VIEW_SUBREGION_ANCHOR_H_

#include "base/callback_list.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace user_education {

// Creates a synthetic anchor element which tracks a subregion of a view.
class ViewSubregionAnchor : public ui::TrackedElement {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // Creates a synthetic help bubble anchor with identifier `id` that tracks the
  // position and visibility of `view`, maintaining a relative position inside
  // the view.
  //
  // The context will be read from `view`, so the view must be on a widget
  // before this is created; this object should also not outlive the view.
  ViewSubregionAnchor(ui::ElementIdentifier id, views::View& view);
  ~ViewSubregionAnchor() override;

  // Specifies the anchor region within the view.
  void MaybeUpdateAnchor(gfx::Rect local_anchor_region);

  // Gets the view associated with this element.
  views::View& view() { return *view_; }

  // Sets a "manually hidden" state in which this anchor does not register as
  // visible even when the underlying view is visible.
  void SetHidden(bool hidden);

  // ui::TrackedElement:
  gfx::Rect GetScreenBounds() const override;
  gfx::NativeView GetNativeView() const override;
  std::string ToString() const override;

 private:
  void OnAnchorViewShown(ui::TrackedElement* el);
  void OnAnchorViewHidden(ui::TrackedElement* el);

  void UpdateVisibility();

  bool visible_ = false;
  bool view_visible_ = false;
  bool manually_hidden_ = false;
  gfx::Rect last_anchor_region_;
  const raw_ref<views::View> view_;
  base::CallbackListSubscription anchor_view_shown_subscription_;
  base::CallbackListSubscription anchor_view_hidden_subscription_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_VIEW_SUBREGION_ANCHOR_H_
