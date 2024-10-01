// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler_view.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace views {
class FlexLayoutView;
class ImageButton;
class LabelButton;
class UniqueWidgetPtr;
}  // namespace views

namespace chromeos::mahi {

// A bubble style view to show Mahi Menu.
class MahiMenuView : public chromeos::editor_menu::PreTargetHandlerView {
  METADATA_HEADER(MahiMenuView, chromeos::editor_menu::PreTargetHandlerView)

 public:
  // Enum that indicates where the Mahi Menu is shown. Different surfaces have
  // different ways of providing content.
  enum class Surface {
    kBrowser,
    kMediaApp,
  };

  explicit MahiMenuView(Surface surface = Surface::kBrowser);
  MahiMenuView(const MahiMenuView&) = delete;
  MahiMenuView& operator=(const MahiMenuView&) = delete;
  ~MahiMenuView() override;

  // Creates a menu widget that contains a `MahiMenuView`, configured with the
  // given `anchor_view_bounds`.
  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      const Surface surface = Surface::kBrowser);

  // Returns the host widget's name.
  static const char* GetWidgetName();

  // chromeos::editor_menu::PreTargetHandlerView:
  void RequestFocus() override;

  // Updates the bounds of the view according to the given `anchor_view_bounds`.
  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

 private:
  class MenuTextfieldController;

  // Buttons callback.
  void OnButtonPressed(::chromeos::mahi::ButtonType button_type);

  // Texfield callback.
  void OnQuestionSubmitted();

  std::unique_ptr<views::FlexLayoutView> CreateInputContainer();

  // Controller for `textfield_`. Enables the
  // `submit_question_button` only when the `textfield_` contains some input.
  // Also, submits a question if the user presses the enter key while focused on
  // the textfield.
  std::unique_ptr<MenuTextfieldController> textfield_controller_;

  raw_ptr<views::ImageButton> settings_button_ = nullptr;
  raw_ptr<views::LabelButton> summary_button_ = nullptr;
  raw_ptr<views::LabelButton> outline_button_ = nullptr;
  raw_ptr<views::Textfield> textfield_ = nullptr;
  raw_ptr<views::ImageButton> submit_question_button_ = nullptr;

  // Where the mahi menu widget is shown, currently it could be the browser (web
  // pages) or the media app (pdf files).
  const Surface surface_;

  base::WeakPtrFactory<MahiMenuView> weak_ptr_factory_{this};
};

}  // namespace chromeos::mahi

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_VIEW_H_
