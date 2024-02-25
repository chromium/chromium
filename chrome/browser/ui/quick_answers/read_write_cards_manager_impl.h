// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_READ_WRITE_CARDS_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_READ_WRITE_CARDS_MANAGER_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_cards_manager.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom-forward.h"

class QuickAnswersControllerImpl;

namespace content {
class BrowserContext;
struct ContextMenuParams;
}  // namespace content

namespace chromeos {

namespace editor_menu {
class EditorMenuControllerImpl;
}  // namespace editor_menu

namespace mahi {
class MahiMenuController;
}  // namespace mahi

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
                       editor_menu::FetchControllerCallback callback) override;

  chromeos::editor_menu::EditorMenuControllerImpl* editor_menu_for_testing() {
    return editor_menu_controller_.get();
  }

 private:
  void OnEditorPanelContextCallback(
      const content::ContextMenuParams& params,
      editor_menu::FetchControllerCallback callback,
      content::BrowserContext* context,
      const crosapi::mojom::EditorPanelContextPtr editor_panel_context);

  base::WeakPtr<chromeos::ReadWriteCardController>
  GetMahiOrQuickAnswerControllerIfEligible(
      const content::ContextMenuParams& params);

  std::unique_ptr<QuickAnswersControllerImpl> quick_answers_controller_;
  std::unique_ptr<chromeos::editor_menu::EditorMenuControllerImpl>
      editor_menu_controller_;
  std::unique_ptr<chromeos::mahi::MahiMenuController> mahi_menu_controller_;

  base::WeakPtrFactory<ReadWriteCardsManagerImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_READ_WRITE_CARDS_MANAGER_IMPL_H_
