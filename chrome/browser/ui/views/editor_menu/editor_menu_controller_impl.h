// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_

#include <memory>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_card_controller.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom-forward.h"
#include "content/public/browser/browser_context.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace chromeos::editor_menu {

// Implementation of ReadWriteCardController. It manages the editor menu related
// views.
class EditorMenuControllerImpl : public chromeos::ReadWriteCardController,
                                 public EditorMenuViewDelegate {
 public:
  EditorMenuControllerImpl();
  EditorMenuControllerImpl(const EditorMenuControllerImpl&) = delete;
  EditorMenuControllerImpl& operator=(const EditorMenuControllerImpl&) = delete;
  ~EditorMenuControllerImpl() override;

  // ReadWriteCardController:
  void OnContextMenuShown(Profile* profile) override;
  void OnTextAvailable(const gfx::Rect& anchor_bounds,
                       const std::string& selected_text,
                       const std::string& surrounding_text) override;
  void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) override;
  void OnDismiss(bool is_other_command_executed) override;

  // EditorMenuViewDelegate:
  void OnSettingsButtonPressed() override;
  void OnChipButtonPressed(std::string_view text_query_id) override;
  void OnTextfieldArrowButtonPressed(std::u16string_view text) override;
  void OnPromoCardWidgetClosed(
      views::Widget::ClosedReason closed_reason) override;
  void OnEditorMenuVisibilityChanged(bool visible) override;

  // As the name suggests this method is used to set the active browser context
  // instance. This method must be invoked prior to showing either of the
  // editor widgets.
  void SetBrowserContext(content::BrowserContext* context);

  views::Widget* editor_menu_widget_for_testing() {
    return editor_menu_widget_.get();
  }

  void OnGetEditorPanelContextResultForTesting(
      const gfx::Rect& anchor_bounds,
      crosapi::mojom::EditorPanelContextPtr context);

 private:
  // Holds any important objects that are scoped to the lifetime of a visible
  // editor card instance (ie promo card, or editor menu card). A session begins
  // once the context menu is shown to the user and one of the editor cards is
  // shown to the user. The session ends when the card is dismissed from the
  // user's view.
  struct EditorCardSession {
    // Provides access to the core editor backend.
    crosapi::mojom::EditorPanelManager& panel_manager;
  };

  void OnGetEditorPanelContextResult(
      const gfx::Rect& anchor_bounds,
      crosapi::mojom::EditorPanelContextPtr context);

  // This method is fired whenever the EditorPromoCard, or EditorMenu cards are
  // hidden from the user's view.
  void OnEditorCardHidden();

  views::UniqueWidgetPtr editor_menu_widget_;

  // May hold the currently active editor card session. If this is nullptr then
  // no session is active.
  std::unique_ptr<EditorCardSession> card_session_;

  base::WeakPtrFactory<EditorMenuControllerImpl> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
