// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SCROLLABLE_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_SCROLLABLE_ELEMENT_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"

#include "ui/gfx/animation/keyframe/transition.h"

namespace vr {

// Allows the element hierarchy within it to be scrolled, providing a windowed
// or clipped view of its descendants, given an orientation and a maximum span.
// It is required that none of its descendants define any rotation. A good use
// case for this element is the creation of menus, for which you can place a
// LinearLayout as its only scrolling.
class VR_UI_EXPORT ScrollableElement : public UiElement {
 public:
  enum Orientation { kVertical, kHorizontal };

  explicit ScrollableElement(Orientation orientation);

  ScrollableElement(const ScrollableElement&) = delete;
  ScrollableElement& operator=(const ScrollableElement&) = delete;

  ~ScrollableElement() override;

  // Sets the maximum size the element can have in the axis of orientation.
  // Makes a call to SetSize. If the max span is big enough to contain the
  // entirety of the inner element, the sizes match and scrolling doesn't have
  // any effect.
  void set_max_span(float span);

  float scroll_offset() const { return scroll_offset_; }

  // UiElement overrides.
  gfx::RectF ComputeContributingChildrenBounds() final;
  void LayOutNonContributingChildren() final;
  void OnSetSize(const gfx::SizeF& size) final;

  void AddScrollingChild(std::unique_ptr<UiElement> child);

  void SetScrollAnchoring(LayoutAlignment anchoring);

  void OnScrollBegin(std::unique_ptr<InputEvent> gesture,
                     const gfx::PointF& position) override;
  void OnScrollUpdate(std::unique_ptr<InputEvent> gesture,
                      const gfx::PointF& position) override;
  void OnScrollEnd(std::unique_ptr<InputEvent> gesture,
                   const gfx::PointF& position) override;

 private:
  using UiElement::AddChild;
  using UiElement::set_bounds_contain_children;
  using UiElement::set_clip_descendants;
  using UiElement::set_scrollable;
  using UiElement::SetSize;

  float ComputeScrollSpan() const;
  void SetInitialScroll();

  raw_ptr<UiElement> inner_element_;

  Orientation orientation_ = kVertical;
  LayoutAlignment scrolling_anchoring_ = NONE;
  float max_span_ = 1.0f;
  gfx::SizeF inner_size_;

  float scroll_offset_ = 0.0f;

  gfx::Transition cached_transition_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SCROLLABLE_ELEMENT_H_
