// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_card_controller.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace chromeos {

class ReadWriteCardsUiController;

namespace mahi {
// Controller that manages the mahi menu related views.
// TODO(b/319256809): Rename this class to something less misleading.
class MahiMenuController : public chromeos::ReadWriteCardController,
                           public chromeos::MahiMediaAppEventsProxy::Observer {
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

  // chromeos::MahiMediaAppEventsProxy::Observer
  void OnPdfContextMenuShown(const gfx::Rect& anchor) override;
  void OnPdfContextMenuHide() override;

  // Returns true if the current focused page is distillable.
  bool IsFocusedPageDistillable();

  // Records metrics of whether the focus page is distillable or not.
  void RecordPageDistillable();

  views::Widget* menu_widget_for_test() { return menu_widget_.get(); }
  base::WeakPtr<MahiMenuController> GetWeakPtr();

  void set_is_distillable_for_testing(bool is_distillable_for_testing) {
    is_distillable_for_testing_ = is_distillable_for_testing;
  }

 private:
  const raw_ref<ReadWriteCardsUiController> read_write_cards_ui_controller_;
  views::UniqueWidgetPtr menu_widget_;

  std::optional<bool> is_distillable_for_testing_;

  base::WeakPtrFactory<MahiMenuController> weak_factory_{this};
};

}  // namespace mahi

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_MENU_CONTROLLER_H_
