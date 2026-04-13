// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_
#define CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/color/color_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageButton;
class MenuRunner;
}  // namespace views

namespace lens {

// The lens preselection bubble gives users info on how to interact with the
// lens overlay.
class LensPreselectionBubble : public views::BubbleDialogDelegateView,
                               public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(LensPreselectionBubble, views::BubbleDialogDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kExitButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonElementId);

  using ExitClickedCallback = views::Button::PressedCallback;
  explicit LensPreselectionBubble(tabs::TabHandle tab_handle,
                                  views::View* anchor_view,
                                  bool offline,
                                  bool show_cancel_button,
                                  ui::ColorId bubble_background_color,
                                  ExitClickedCallback exit_clicked_callback,
                                  base::OnceClosure on_cancel_callback);
  ~LensPreselectionBubble() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  // Reset the label text on the preselection bubble to the new `string_id`.
  // Also makes sure the bubble resizes and the accessible title is also
  // changed.
  void SetLabelText(int string_id);

  // Set the icon on the preselection bubble to the new `icon`.
  void SetIcon(const gfx::VectorIcon& icon);

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

  tabs::TabHandle tab_handle_;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  // More info button. Only shown when search bubble enabled.
  raw_ptr<views::ImageButton> more_info_button_ = nullptr;
  // Button shown in bubble to close lens overlay. Only shown in offline state.
  raw_ptr<views::MdTextButton> exit_button_ = nullptr;
  // Whether to show cancel button.
  const bool show_cancel_button_;
  // Button shown in bubble to cancel selection.
  raw_ptr<views::MdTextButton> cancel_button_ = nullptr;
  // Whether user is offline.
  bool offline_ = false;
  // Background color of the bubble.
  const ui::ColorId bubble_background_color_;
  // Callback for exit button which closes the lens overlay.
  ExitClickedCallback exit_clicked_callback_;
  // Model for the more info menu.
  std::unique_ptr<ui::MenuModel> more_info_menu_model_;
  // Runner for the more info menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_
