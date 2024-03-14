// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONTROLLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_card_controller.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace chromeos {

class ReadWriteCardsUiController;

namespace mahi {
// Controller that manages the mahi menu related views.
// TODO(b/319256809): Rename this class to something less misleading.
class MahiMenuController : public chromeos::ReadWriteCardController {
 public:
  explicit MahiMenuController(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller);
  MahiMenuController(const MahiMenuController&) = delete;
  MahiMenuController& operator=(const MahiMenuController&) = delete;
  ~MahiMenuController() override;

  // chromeos::ReadWriteCardController:
  void OnContextMenuShown(Profile* profile) override;
  void OnTextAvailable(const gfx::Rect& anchor_bounds,
                       const std::string& selected_text,
                       const std::string& surrounding_text) override;
  void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) override;
  void OnDismiss(bool is_other_command_executed) override;

  views::Widget* menu_widget_for_test() { return menu_widget_.get(); }
  base::WeakPtr<MahiMenuController> GetWeakPtr();

 private:
  ReadWriteCardsUiController& read_write_cards_ui_controller_;
  views::UniqueWidgetPtr menu_widget_;
  base::WeakPtrFactory<MahiMenuController> weak_factory_{this};
};

}  // namespace mahi

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONTROLLER_H_
