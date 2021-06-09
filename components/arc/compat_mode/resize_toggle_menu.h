// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_
#define COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class BubbleDialogDelegateView;
class Button;
}  // namespace views

namespace arc {

class ArcResizeLockPrefDelegate;

class ResizeToggleMenu : public views::WidgetObserver {
 public:
  enum class CommandId {
    kResizePhone,
    kResizeTablet,
    kResizeDesktop,
    kOpenSettings,
  };

  ResizeToggleMenu(views::Widget* widget,
                   ArcResizeLockPrefDelegate* pref_delegate);
  ResizeToggleMenu(const ResizeToggleMenu&) = delete;
  ResizeToggleMenu& operator=(const ResizeToggleMenu&) = delete;
  ~ResizeToggleMenu() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  friend class ResizeToggleMenuTest;

  void ExecuteCommand(CommandId command_id);

  gfx::Rect GetAnchorRect() const;

  std::unique_ptr<views::BubbleDialogDelegateView> MakeBubbleDelegateView(
      views::Widget* parent,
      gfx::Rect anchor_rect,
      base::RepeatingCallback<void(CommandId)> command_handler);

  views::Widget* widget_;

  ArcResizeLockPrefDelegate* pref_delegate_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  // Store only for testing.
  views::Widget* bubble_widget_{nullptr};
  views::Button* phone_button_{nullptr};
  views::Button* tablet_button_{nullptr};
  views::Button* desktop_button_{nullptr};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_H_
