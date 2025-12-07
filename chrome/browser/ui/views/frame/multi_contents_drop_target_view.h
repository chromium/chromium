// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/dragging/tab_drag_controller.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace views {
class FlexLayout;
class ImageView;
class Label;
}  // namespace views

// MultiContentsDropTargetView shows a drop target view used for the drag and
// drop link interaction to create a split view.
class MultiContentsDropTargetView : public views::View,
                                    public views::AnimationDelegateViews {
  METADATA_HEADER(MultiContentsDropTargetView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMultiContentsDropTargetElementId);

  // Represents which edge of the contents area the drop target is on.
  enum class DropSide {
    START = 0,
    END = 1,
  };

  // Represents the state of the drop target which determines its size.
  enum class DropTargetState {
    // A small target that is just a sliver on the side of the screen.
    kNudge,
    // A medium-sized target that is between the nudge and full states.
    kNudgeToFull,
    // A large target that takes up a significant portion of the screen.
    kFull,
  };

  // Represents the type of content being dragged.
  enum class DragType {
    kTab,
    kLink,
  };

  // Delegate for handling drag events that are routed to this view.
  class DragDelegate {
   public:
    virtual ~DragDelegate() = default;

    virtual bool GetDropFormats(
        int* formats,
        std::set<ui::ClipboardFormatType>* format_types) = 0;
    virtual bool CanDrop(const ui::OSExchangeData& data) = 0;
    virtual void OnDragEntered(const ui::DropTargetEvent& event) = 0;
    virtual void OnDragExited() = 0;
    virtual void OnDragDone() = 0;
    virtual int OnDragUpdated(const ui::DropTargetEvent& event) = 0;
    virtual views::View::DropCallback GetDropCallback(
        const ui::DropTargetEvent& event) = 0;
  };

  MultiContentsDropTargetView();
  MultiContentsDropTargetView(const MultiContentsDropTargetView&) = delete;
  MultiContentsDropTargetView& operator=(const MultiContentsDropTargetView&) =
      delete;
  ~MultiContentsDropTargetView() override;

  void SetDragDelegate(DragDelegate* drag_delegate);

  void Show(DropSide side, DropTargetState state, DragType drag_type);
  void Hide(bool suppress_animation_ = false);

  bool IsClosing() const;

  // Returns the preferred width of this view for the given web contents width,
  // considering animation progress.
  int GetPreferredWidth(int web_contents_width) const;
  // Returns the maximum width that a view should be for the given web
  // contents width.
  static int GetMaxWidth(int web_contents_width,
                         DropTargetState state,
                         DragType drag_type);

  // views::View
  void SetVisible(bool visible) override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  void OnDragDone() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  void HandleTabDrop(TabDragDelegate::DragController& controller);

  std::optional<DropSide> side() const { return side_; }
  std::optional<DragType> drag_type() const { return drag_type_; }
  std::optional<DropTargetState> state() const { return state_; }

  raw_ptr<views::ImageView> icon_view_for_testing() { return icon_view_; }
  raw_ptr<views::Label> label_for_testing() { return label_; }
  gfx::SlideAnimation& animation_for_testing() { return animation_; }

  bool ShouldShowAnimation() const;

 private:
  void UpdateVisibility(bool should_be_open);

  double GetAnimationValue() const;

  void DoDrop(const ui::DropTargetEvent& event,
              ui::mojom::DragOperation& output_drag_op,
              std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // The side that this view is showing on.
  std::optional<DropSide> side_ = std::nullopt;

  // The state that this view is in, if showing.
  std::optional<DropTargetState> state_ = std::nullopt;

  // The type of drag operation.
  std::optional<DragType> drag_type_ = std::nullopt;

  raw_ptr<DragDelegate> drag_delegate_ = nullptr;

  // Animation controlling showing and hiding of the drop target view.
  gfx::SlideAnimation animation_{this};
  // The width at the time the animation started.
  // This is relevant for transitioning from a nudge state to a full state.
  std::optional<int> animate_expand_starting_width_ = std::nullopt;
  // Flag to used by base::AutoReset to suppress animation for hide request
  // to avoid content reflows during the split view creation.
  bool should_suppress_animation_ = false;

  raw_ptr<views::View> inner_container_ = nullptr;
  raw_ptr<views::FlexLayout> inner_container_layout_ = nullptr;

  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_
