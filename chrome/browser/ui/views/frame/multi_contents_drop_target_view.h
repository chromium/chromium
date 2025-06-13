// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
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

  class DropDelegate {
   public:
    virtual ~DropDelegate() = default;

    // Handles links that are dropped on the view.
    virtual void HandleLinkDrop(DropSide side,
                                const std::vector<GURL>& urls) = 0;
  };

  explicit MultiContentsDropTargetView(DropDelegate& drop_delegate);
  MultiContentsDropTargetView(const MultiContentsDropTargetView&) = delete;
  MultiContentsDropTargetView& operator=(const MultiContentsDropTargetView&) =
      delete;
  ~MultiContentsDropTargetView() override;

  double GetAnimationValue() const;

  void Show(DropSide side);
  void Hide();

  bool IsClosing() const;

  // Returns the preferred width of this view, considering animation progress.
  int GetPreferredWidth() const;

  // views::View
  void SetVisible(bool visible) override;
  void OnThemeChanged() override;
  bool GetDropFormats(int* formats,
                      std::set<ui::ClipboardFormatType>* format_types) override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  void OnDragDone() override;
  views::View::DropCallback GetDropCallback(
      const ui::DropTargetEvent& event) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  std::optional<DropSide> side() const { return side_; }

  raw_ptr<views::ImageView> icon_view_for_testing() { return icon_view_; }
  gfx::SlideAnimation& animation_for_testing() { return animation_; }

 private:
  void UpdateVisibility(bool should_be_open);

  bool ShouldShowAnimation() const;

  void DoDrop(const ui::DropTargetEvent& event,
              ui::mojom::DragOperation& output_drag_op,
              std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // The side that this view is showing on.
  std::optional<DropSide> side_ = std::nullopt;

  const raw_ref<DropDelegate> drop_delegate_;

  // Animation controlling showing and hiding of the drop target view.
  gfx::SlideAnimation animation_{this};

  raw_ptr<views::View> inner_container_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_
