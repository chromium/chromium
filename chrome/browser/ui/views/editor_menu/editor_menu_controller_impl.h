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
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_card_controller.h"
#include "chrome/browser/ui/views/editor_menu/editor_manager.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "content/public/browser/browser_context.h"

namespace views {
class Widget;
}

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

  bool SetBrowserContext(content::BrowserContext* context);
  void LogEditorMode(const EditorMode& editor_mode);
  void GetEditorContext(
      base::OnceCallback<void(const EditorContext&)> callback);
  void DismissCard();
  void TryCreatingEditorSession();

  views::Widget* editor_menu_widget_for_testing() {
    return editor_menu_widget_.get();
  }

  void OnGetAnchorBoundsAndEditorContextForTesting(
      const gfx::Rect& anchor_bounds,
      const EditorContext& context);

  base::WeakPtr<EditorMenuControllerImpl> GetWeakPtr();

 private:
  // Holds any important objects that are scoped to the lifetime of a visible
  // editor card instance (ie promo card, or editor menu card). A session begins
  // once the context menu is shown to the user and one of the editor cards is
  // shown to the user. The session ends when the card is dismissed from the
  // user's view.
  class EditorCardSession : public EditorManager::Observer {
   public:
    class Delegate {
     public:
      virtual void DismissCard() = 0;
    };

    explicit EditorCardSession(EditorMenuControllerImpl* controller,
                               std::unique_ptr<EditorManager> editor_manager);
    ~EditorCardSession() override;

    // EditorManager::Observer overrides
    void OnEditorModeChanged(const EditorMode& mode) override;

    EditorManager& manager();

   private:
    // Not owned by this class
    raw_ptr<EditorMenuControllerImpl> controller_;

    // Provides access to the core editor backend.
    std::unique_ptr<EditorManager> manager_;
  };

  void OnGetEditorContext(
      base::OnceCallback<void(const EditorContext&)> callback,
      const EditorContext& context);
  void OnGetAnchorBoundsAndEditorContext(const gfx::Rect& anchor_bounds,
                                         const EditorContext& context);

  // This method is fired whenever the EditorPromoCard, or EditorMenu cards are
  // hidden from the user's view.
  void OnEditorCardHidden();

  // Disables the editor menu. We do this when we don't want the editor menu
  // buttons or textfield to receive keyboard or mouse input.
  void DisableEditorMenu();

  std::unique_ptr<views::Widget> editor_menu_widget_;

  // May hold the currently active editor card session. If this is nullptr then
  // no session is active.
  std::unique_ptr<EditorCardSession> card_session_;

  base::WeakPtrFactory<EditorMenuControllerImpl> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
