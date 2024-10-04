// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_
#define CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace views {
class ImageButton;
class MenuRunner;
}  // namespace views

class LensOverlayController;

namespace lens {

// The lens preselection bubble gives users info on how to interact with the
// lens overlay.
class LensPreselectionBubble : public views::BubbleDialogDelegateView,
                               public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(LensPreselectionBubble, views::BubbleDialogDelegateView)

 public:
  using ExitClickedCallback = views::Button::PressedCallback;
  explicit LensPreselectionBubble(
      base::WeakPtr<LensOverlayController> lens_overlay_controller,
      views::View* anchor_view,
      bool offline,
      ExitClickedCallback callback);
  ~LensPreselectionBubble() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  // Reset the label text on the preselection bubble to the new `string_id`.
  // Also makes sure the bubble resizes and the accessible title is also
  // changed.
  void SetLabelText(int string_id);

  enum CommandID {
    COMMAND_MY_ACTIVITY,
    COMMAND_LEARN_MORE,
    COMMAND_SEND_FEEDBACK,
  };

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 protected:
  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  void OnThemeChanged() override;

 private:
  // Opens the more info menu.
  void OpenMoreInfoMenu();

  // Weak pointer to the overlay controller for routing more info menu options.
  // The overlay controller manages the lifetime of the bubble's owner and
  // should always outlive it.
  const base::WeakPtr<LensOverlayController> lens_overlay_controller_;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  // More info button. Only shown when search bubble enabled.
  raw_ptr<views::ImageButton> more_info_button_ = nullptr;
  // Button shown in bubble to close lens overlay. Only shown in offline state.
  raw_ptr<views::MdTextButton> exit_button_ = nullptr;
  // Whether user is offline.
  bool offline_ = false;
  // Callback for exit button which closes the lens overlay.
  ExitClickedCallback callback_;
  // Model for the more info menu.
  std::unique_ptr<ui::MenuModel> more_info_menu_model_;
  // Runner for the more info menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_
