// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_

#include <memory>
#include <string_view>

#include "ash/lobster/lobster_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ash/editor_menu/editor_manager.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_view_delegate.h"
#include "chrome/browser/ui/ash/editor_menu/lobster_manager.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_card_controller.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "content/public/browser/browser_context.h"

class ApplicationLocaleStorage;
class Profile;

namespace views {
class Widget;
}

namespace chromeos::editor_menu {

// Implementation of ReadWriteCardController. It manages the editor menu related
// views.
class EditorMenuControllerImpl : public chromeos::ReadWriteCardController,
                                 public EditorMenuViewDelegate {
 public:
  explicit EditorMenuControllerImpl(
      const ApplicationLocaleStorage* application_locale_storage);
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
  void OnTabSelected(int index) override;
  void OnTextfieldArrowButtonPressed(std::u16string_view text) override;
  void OnPromoCardWidgetClosed(
      views::Widget::ClosedReason closed_reason) override;
  void OnEditorMenuVisibilityChanged(bool visible,
                                     bool destroy_session) override;

  bool SetBrowserContext(content::BrowserContext* context);
  void LogEditorMode(const EditorMode& editor_mode);
  void GetEditorMenuCardContext(
      base::OnceCallback<void(const EditorMenuCardContext&)> callback);
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
    enum class ActiveFeature { kEditor, kLobster };

    explicit EditorCardSession(EditorMenuControllerImpl* controller,
                               std::unique_ptr<EditorManager> editor_manager,
                               std::unique_ptr<LobsterManager> lobster_manager);
    ~EditorCardSession() override;

    // EditorManager::Observer overrides
    void OnEditorModeChanged(EditorMode mode) override;

    void StartFlowWithFreeformText(const std::string& freeform_text);
    void StartFlowWithPreset(const std::string& preset_id);

    void OpenSettings();

    EditorManager* editor_manager() const { return editor_manager_.get(); }

    LobsterManager* lobster_manager() const { return lobster_manager_.get(); }

    LobsterMode GetLobsterMode() const;

    void SetActiveFeature(ActiveFeature active_feature);

    void OnSelectedTextChanged(const std::string& text);

    ActiveFeature active_feature = ActiveFeature::kEditor;

   private:
    // Not owned by this class
    raw_ptr<EditorMenuControllerImpl> controller_;

    // Provides access to the core editor backend.
    std::unique_ptr<EditorManager> editor_manager_;

    // Provides access to the lobster trigger.
    std::unique_ptr<LobsterManager> lobster_manager_;

    // The current selected text.
    std::string selected_text_;
  };

  void OnGetEditorCardMenuContext(
      base::OnceCallback<void(const EditorMenuCardContext&)> callback,
      LobsterMode lobster_mode,
      const EditorContext& context);
  void OnGetAnchorBoundsAndEditorContext(const gfx::Rect& anchor_bounds,
                                         LobsterMode lobster_mode,
                                         const EditorContext& context);

  // This method is fired whenever the EditorPromoCard, or EditorMenu cards are
  // hidden from the user's view.
  void OnEditorCardHidden(bool destroy_session = true);

  // Disables the editor menu. We do this when we don't want the editor menu
  // buttons or textfield to receive keyboard or mouse input.
  void DisableEditorMenu();

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;

  std::unique_ptr<views::Widget> editor_menu_widget_;

  // May hold the currently active editor card session. If this is nullptr then
  // no session is active.
  std::unique_ptr<EditorCardSession> card_session_;

  base::WeakPtrFactory<EditorMenuControllerImpl> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_ASH_EDITOR_MENU_EDITOR_MENU_CONTROLLER_IMPL_H_
