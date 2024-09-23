// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_FRAME_HEADER_H_
#define CHROMEOS_UI_FRAME_FRAME_HEADER_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, BackButtonAlignment);
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, TitleIconAlignment);
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, FrameColors);
class FramePaintWaiter;
}  // namespace ash

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace ui {
class Layer;
class LayerTreeOwner;
}  // namespace ui

namespace views {
enum class CaptionButtonLayoutSize;
class FrameCaptionButton;
class NonClientFrameView;
class View;
class Widget;
}  // namespace views

namespace chromeos {

class CaptionButtonModel;
class FrameCenterButton;
class FrameCaptionButtonContainerView;

// Helper class for managing the window header.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) FrameHeader
    : public ui::LayerOwner::Observer {
 public:
  // An invisible view that drives the frame's animation. This holds the
  // animating layer as a layer beneath this view so that it's behind all other
  // child layers of the window to avoid hiding their contents.
  class FrameAnimatorView : public views::View,
                            public views::ViewObserver,
                            public ui::ImplicitAnimationObserver {
    METADATA_HEADER(FrameAnimatorView, views::View)

   public:
    explicit FrameAnimatorView(views::View* parent);
    FrameAnimatorView(const FrameAnimatorView&) = delete;
    FrameAnimatorView& operator=(const FrameAnimatorView&) = delete;
    ~FrameAnimatorView() override;

    void StartAnimation(base::TimeDelta duration);

    // views::Views:
    std::unique_ptr<ui::Layer> RecreateLayer() override;
    void LayerDestroyed(ui::Layer* layer) override;

    // ViewObserver:
    void OnChildViewReordered(views::View* observed_view,
                              views::View* child) override;
    void OnViewBoundsChanged(views::View* observed_view) override;

    // ui::ImplicitAnimationObserver overrides:
    void OnImplicitAnimationsCompleted() override;

   private:
    void StopAnimation();

    raw_ptr<views::View> parent_;
    std::unique_ptr<ui::LayerTreeOwner> layer_owner_;
  };

  enum Mode { MODE_ACTIVE, MODE_INACTIVE };

  static FrameHeader* Get(views::Widget* widget);

  // Moves the client view in front of the frame animator view. This allows the
  // frame animator view to still be at the bottom of the z-order while also
  // keeping the rest of the frame view's children on top of the client view.
  static views::View::Views GetAdjustedChildrenInZOrder(
      views::NonClientFrameView* frame_view);

  FrameHeader(const FrameHeader&) = delete;
  FrameHeader& operator=(const FrameHeader&) = delete;

  ~FrameHeader() override;

  const std::u16string& frame_text_override() const {
    return frame_text_override_;
  }

  // Returns the header's minimum width.
  int GetMinimumHeaderWidth() const;

  // Paints the header.
  void PaintHeader(gfx::Canvas* canvas);

  // Performs layout for the header.
  void LayoutHeader();

  // Invalidate layout for the header.
  void InvalidateLayout();

  // Get the height of the header.
  int GetHeaderHeight() const;

  // Gets / sets how much of the header is painted. This allows the header to
  // paint under things (like the tabstrip) which have transparent /
  // non-painting sections. This height does not affect LayoutHeader().
  int GetHeaderHeightForPainting() const;
  void SetHeaderHeightForPainting(int height_for_painting);

  // Schedule a re-paint of the entire title.
  void SchedulePaintForTitle();

  // True to instruct the frame header to paint the header as an active
  // state.
  virtual void SetPaintAsActive(bool paint_as_active);

  // Called when frame show state is changed.
  void OnShowStateChanged(ui::mojom::WindowShowState show_state);

  void OnFloatStateChanged();

  // Set/Get the radius of top-left and top-right corners of the header.
  int header_corner_radius() const { return corner_radius_; }
  void SetHeaderCornerRadius(int radius);

  void SetLeftHeaderView(views::View* view);
  void SetBackButton(views::FrameCaptionButton* view);
  void SetCenterButton(chromeos::FrameCenterButton* view);
  views::FrameCaptionButton* GetBackButton() const;
  chromeos::FrameCenterButton* GetCenterButton() const;
  const chromeos::CaptionButtonModel* GetCaptionButtonModel() const;

  // Updates the frame header painting to reflect a change in frame colors and a
  // change in mode.
  virtual void UpdateFrameColors() = 0;

  // Returns window mask for the rounded corner of the frame header.
  virtual SkPath GetWindowMaskForFrameHeader(const gfx::Size& size);

  // Sets text to display in place of the window's title. This will be shown
  // regardless of what ShouldShowWindowTitle() returns.
  void SetFrameTextOverride(const std::u16string& frame_text_override);

  void UpdateFrameHeaderKey();

  // Adds the layer owned by layer_owner to the kbelow LayerRegion of the frame
  // header view, so that the layer can be always below the layer of frame
  // header regardless of the view hierarchy.
  // It is the caller's responsibility to call RemoveLayerBeneath(), when the
  // layer_owner or the layer owned by layer_owner is destroyed, or when
  // the layer is removed from its parent; so that view::ReorderChildLayers()
  // can function properly when it adjusts the children layers order of the
  // parent of frame header view.
  void AddLayerBeneath(ui::LayerOwner* layer_owner);
  // Removes the effect of AddLayerBeneath().
  void RemoveLayerBeneath();

  views::View* view() { return view_; }

  chromeos::FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

  // ui::LayerOwner::Observer overrides:
  void OnLayerRecreated(ui::Layer* old_layer) override;

 protected:
  FrameHeader(views::Widget* target_widget, views::View* view);

  views::Widget* target_widget() { return target_widget_; }
  const views::Widget* target_widget() const { return target_widget_; }

  // Returns bounds of the region in |view_| which is painted with the header
  // images. The region is assumed to start at the top left corner of |view_|
  // and to have the same width as |view_|.
  gfx::Rect GetPaintedBounds() const;

  void UpdateCaptionButtonColors(std::optional<ui::ColorId> icon_color_id);

  void PaintTitleBar(gfx::Canvas* canvas);

  void SetCaptionButtonContainer(
      chromeos::FrameCaptionButtonContainerView* caption_button_container);

  Mode mode() const { return mode_; }

  virtual void DoPaintHeader(gfx::Canvas* canvas) = 0;
  virtual views::CaptionButtonLayoutSize GetButtonLayoutSize() const = 0;
  virtual SkColor GetTitleColor() const = 0;
  virtual SkColor GetCurrentFrameColor() const = 0;

  // Starts fade transition animation with given duration.
  void StartTransitionAnimation(base::TimeDelta duration);

  ui::ColorId GetColorIdForCurrentMode() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, BackButtonAlignment);
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, TitleIconAlignment);
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, FrameColors);
  friend class ash::FramePaintWaiter;

  void LayoutHeaderInternal();

  gfx::Rect GetTitleBounds() const;

  // The widget that the caption buttons act on. This can be different from
  // |view_|'s widget.
  raw_ptr<views::Widget> target_widget_;

  // The view into which |this| paints.
  raw_ptr<views::View, DanglingUntriaged> view_;
  raw_ptr<views::FrameCaptionButton, DanglingUntriaged> back_button_ =
      nullptr;  // May remain nullptr.
  raw_ptr<views::View, DanglingUntriaged> left_header_view_ =
      nullptr;  // May remain nullptr.
  raw_ptr<chromeos::FrameCaptionButtonContainerView> caption_button_container_ =
      nullptr;
  raw_ptr<FrameAnimatorView, DanglingUntriaged> frame_animator_ =
      nullptr;  // owned by view tree.
  raw_ptr<chromeos::FrameCenterButton, DanglingUntriaged> center_button_ =
      nullptr;  // May remain nullptr.

  // The height of the header to paint.
  int painted_height_ = 0;

  // Used to skip animation when the frame hasn't painted yet.
  bool painted_ = false;

  // Layer owner to keep track of the layer that's put beneath the frame header
  // view.
  raw_ptr<ui::LayerOwner> underneath_layer_owner_ = nullptr;

  // Whether the header should be painted as active.
  Mode mode_ = MODE_INACTIVE;

  // The radius of the top-left and top-right corners of the header.
  int corner_radius_ = 0;

  std::u16string frame_text_override_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_FRAME_HEADER_H_
