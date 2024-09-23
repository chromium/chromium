// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_manager.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_controller.h"

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

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;

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
  void TryCreatingEditorSession(const content::ContextMenuParams& params,
                                content::BrowserContext* context) override;

  chromeos::editor_menu::EditorMenuControllerImpl* editor_menu_for_testing() {
    return editor_menu_controller_.get();
  }

 private:
  friend class ReadWriteCardsManagerImplTest;

  void OnGetEditorContext(const content::ContextMenuParams& params,
                          editor_menu::FetchControllersCallback callback,
                          const editor_menu::EditorContext& editor_context);

  // Get the controllers that should be fetched into `FetchController`.
  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>> GetControllers(
      const content::ContextMenuParams& params,
      const editor_menu::EditorContext& editor_context);

  // Helper function to get the Mahi and/or Quick Answers controllers that need
  // to be fetched.
  std::vector<base::WeakPtr<chromeos::ReadWriteCardController>>
  GetQuickAnswersAndMahiControllers(const content::ContextMenuParams& params);

  // Whether we should show Quick Answers or Mahi card, depending on the given
  // context menu params.
  bool ShouldShowQuickAnswers(const content::ContextMenuParams& params);
  bool ShouldShowMahi(const content::ContextMenuParams& params);

  // Gets the opt-in features that Magic Boost should opt-in. Returns a nullopt
  // if we should not initiate an opt-in flow.
  std::optional<OptInFeatures> GetMagicBoostOptInFeatures(
      const content::ContextMenuParams& params,
      const editor_menu::EditorContext& editor_context);

  // `chromeos::ReadWriteCardsUiController` MUST be destructed after
  // `QuickAnswersUiController`, which is owned by `QuickAnswersControllerImpl`.
  // A destructor of `QuickAnswersUiController` accesses
  // `ReadWriteCardsUiController`.
  chromeos::ReadWriteCardsUiController ui_controller_;

  std::unique_ptr<QuickAnswersControllerImpl> quick_answers_controller_;
  std::unique_ptr<chromeos::editor_menu::EditorMenuControllerImpl>
      editor_menu_controller_;
  std::optional<chromeos::mahi::MahiMenuController> mahi_menu_controller_;
  std::optional<chromeos::MagicBoostCardController>
      magic_boost_card_controller_;

  base::WeakPtrFactory<ReadWriteCardsManagerImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_MANAGER_IMPL_H_
