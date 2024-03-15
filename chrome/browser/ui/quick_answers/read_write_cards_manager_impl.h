// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_READ_WRITE_CARDS_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_READ_WRITE_CARDS_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_cards_manager.h"

class QuickAnswersControllerImpl;

namespace content {
class BrowserContext;
struct ContextMenuParams;
}  // namespace content

namespace chromeos {

namespace editor_menu {
class EditorMenuControllerImpl;
}  // namespace editor_menu

class ReadWriteCardController;

// `ReadWriteCardsManagerImpl` provides supported UI controller to given context
// menu params. It could be either QuickAnswersController or
// EditorMenuController, or nullptr.
class ReadWriteCardsManagerImpl : public ReadWriteCardsManager {
 public:
  ReadWriteCardsManagerImpl();
  ReadWriteCardsManagerImpl(const ReadWriteCardsManagerImpl&) = delete;
  ReadWriteCardsManagerImpl& operator=(const ReadWriteCardsManagerImpl&) =
      delete;
  ~ReadWriteCardsManagerImpl() override;

  // ReadWriteCardController:
  void FetchController(const content::ContextMenuParams& params,
                       content::BrowserContext* context,
                       editor_menu::FetchControllersCallback callback) override;
  void SetContextMenuBounds(const gfx::Rect& context_menu_bounds) override;

  chromeos::editor_menu::EditorMenuControllerImpl* editor_menu_for_testing() {
    return editor_menu_controller_.get();
  }

 private:
  friend class ReadWriteCardsManagerImplTest;

  void OnGetEditorModeResult(const content::ContextMenuParams& params,
                             editor_menu::FetchControllersCallback callback,
                             editor_menu::EditorMode editor_mode);

  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
  GetMahiOrQuickAnswerControllersIfEligible(
      const content::ContextMenuParams& params);

  chromeos::ReadWriteCardsUiController ui_controller_;

  std::unique_ptr<QuickAnswersControllerImpl> quick_answers_controller_;
  std::unique_ptr<chromeos::editor_menu::EditorMenuControllerImpl>
      editor_menu_controller_;
  std::optional<chromeos::mahi::MahiMenuController> mahi_menu_controller_;

  base::WeakPtrFactory<ReadWriteCardsManagerImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_READ_WRITE_CARDS_MANAGER_IMPL_H_
