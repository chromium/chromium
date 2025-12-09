// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_controller_impl.h"

#include <memory>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/shell.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/ash/lobster/lobster_service_provider.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/editor_menu/editor_manager_factory.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_view.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace chromeos::editor_menu {

namespace {

ui::TextInputClient* GetCurrentTextInputClient() {
  const ui::InputMethod* input_method =
      ash::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();

  return input_method != nullptr ? input_method->GetTextInputClient() : nullptr;
}

std::unique_ptr<LobsterManager> CreateLobsterManager() {
  ash::LobsterController* lobster_controller =
      ash::Shell::Get()->lobster_controller();

  if (!lobster_controller) {
    return nullptr;
  }

  std::unique_ptr<ash::LobsterController::Trigger> lobster_trigger =
      lobster_controller->CreateTrigger(ash::LobsterEntryPoint::kRightClickMenu,
                                        GetCurrentTextInputClient());

  if (!lobster_trigger) {
    return nullptr;
  }

  return std::make_unique<LobsterManager>(std::move(lobster_trigger));
}

}  // namespace

EditorMenuControllerImpl::EditorMenuControllerImpl(
    const ApplicationLocaleStorage* application_locale_storage)
    : application_locale_storage_(CHECK_DEREF(application_locale_storage)) {}

EditorMenuControllerImpl::~EditorMenuControllerImpl() = default;

void EditorMenuControllerImpl::OnContextMenuShown(Profile* profile) {}

void EditorMenuControllerImpl::OnTextAvailable(
    const gfx::Rect& anchor_bounds,
    const std::string& selected_text,
    const std::string& surrounding_text) {
  if (!card_session_) {
    return;
  }

  card_session_->OnSelectedTextChanged(selected_text);

  LobsterMode lobster_mode = card_session_->GetLobsterMode();

  if (card_session_->editor_manager() == nullptr) {
    OnGetAnchorBoundsAndEditorContext(
        anchor_bounds, lobster_mode,
        EditorContext(EditorMode::kHardBlocked,
                      /*text_selection_mode=*/selected_text.length() > 0
                          ? EditorTextSelectionMode::kHasSelection
                          : EditorTextSelectionMode::kNoSelection,
                      /*consent_status_settled=*/false,
                      /*preset_queries=*/{}));
    return;
  }

  card_session_->editor_manager()->GetEditorPanelContext(base::BindOnce(
      &EditorMenuControllerImpl::OnGetAnchorBoundsAndEditorContext,
      weak_factory_.GetWeakPtr(), anchor_bounds, lobster_mode));
}

void EditorMenuControllerImpl::OnAnchorBoundsChanged(
    const gfx::Rect& anchor_bounds) {
  if (!editor_menu_widget_) {
    return;
  }

  auto* editor_menu_view = editor_menu_widget_->GetContentsView();
  if (views::IsViewClass<EditorMenuView>(editor_menu_view)) {
    views::AsViewClass<EditorMenuView>(editor_menu_view)
        ->UpdateBounds(anchor_bounds);
  } else if (views::IsViewClass<EditorMenuPromoCardView>(editor_menu_view)) {
    views::AsViewClass<EditorMenuPromoCardView>(editor_menu_view)
        ->UpdateBounds(anchor_bounds);
  }
}

void EditorMenuControllerImpl::OnDismiss(bool is_other_command_executed) {
  if (editor_menu_widget_ && !editor_menu_widget_->IsActive()) {
    auto* const editor_menu_view = editor_menu_widget_->GetContentsView();
    if (views::IsViewClass<EditorMenuView>(editor_menu_view)) {
      views::AsViewClass<EditorMenuView>(editor_menu_view)
          ->OnAnchorMenuDismissed();
    } else if (views::IsViewClass<EditorMenuPromoCardView>(editor_menu_view)) {
      views::AsViewClass<EditorMenuPromoCardView>(editor_menu_view)
          ->OnAnchorMenuDismissed();
    }

    editor_menu_widget_.reset();
  }
}

void EditorMenuControllerImpl::OnSettingsButtonPressed() {
  if (!card_session_) {
    return;
  }

  card_session_->OpenSettings();
}

void EditorMenuControllerImpl::OnChipButtonPressed(
    std::string_view text_query_id) {
  if (!card_session_) {
    return;
  }

  DisableEditorMenu();
  card_session_->StartFlowWithPreset(std::string(text_query_id));
}

void EditorMenuControllerImpl::OnTabSelected(int index) {
  if (card_session_ == nullptr) {
    return;
  }

  card_session_->SetActiveFeature(
      index == 1 ? EditorCardSession::ActiveFeature::kLobster
                 : EditorCardSession::ActiveFeature::kEditor);
}

void EditorMenuControllerImpl::OnTextfieldArrowButtonPressed(
    std::u16string_view text) {
  if (text.empty() || card_session_ == nullptr) {
    return;
  }

  DisableEditorMenu();

  card_session_->StartFlowWithFreeformText(base::UTF16ToUTF8(text));
}

void EditorMenuControllerImpl::OnPromoCardWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  if (!card_session_ || card_session_->editor_manager() == nullptr) {
    return;
  }

  switch (closed_reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      card_session_->editor_manager()->StartEditingFlow();
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      card_session_->editor_manager()->OnPromoCardDeclined();
      break;
    default:
      card_session_->editor_manager()->OnPromoCardDismissed();
      break;
  }

  OnEditorCardHidden();
}

void EditorMenuControllerImpl::OnEditorMenuVisibilityChanged(
    bool visible,
    bool destroy_session) {
  if (!card_session_ || card_session_->editor_manager() == nullptr) {
    return;
  }

  card_session_->editor_manager()->OnEditorMenuVisibilityChanged(visible);

  if (!visible) {
    OnEditorCardHidden(destroy_session);
  }
}

bool EditorMenuControllerImpl::SetBrowserContext(
    content::BrowserContext* context) {
  std::unique_ptr<EditorManager> editor_manager = CreateEditorManager(context);
  std::unique_ptr<LobsterManager> lobster_manager = CreateLobsterManager();

  if (editor_manager || lobster_manager) {
    card_session_ = std::make_unique<EditorCardSession>(
        this, std::move(editor_manager), std::move(lobster_manager));
    return true;
  }
  card_session_ = nullptr;
  return false;
}

void EditorMenuControllerImpl::DismissCard() {
  if (editor_menu_widget_) {
    editor_menu_widget_.reset();
  }
}

void EditorMenuControllerImpl::TryCreatingEditorSession() {
  if (!card_session_ || card_session_->editor_manager() == nullptr) {
    return;
  }
  card_session_->editor_manager()->RequestCacheContext();
}

void EditorMenuControllerImpl::LogEditorMode(const EditorMode& editor_mode) {
  if (!card_session_ || card_session_->editor_manager() == nullptr) {
    return;
  }
  card_session_->editor_manager()->LogEditorMode(editor_mode);
}

void EditorMenuControllerImpl::GetEditorMenuCardContext(
    base::OnceCallback<void(const EditorMenuCardContext&)> callback) {
  if (card_session_ == nullptr) {
    return;
  }

  if (card_session_->editor_manager() == nullptr) {
    OnGetEditorCardMenuContext(
        std::move(callback), card_session_->GetLobsterMode(),
        // At this stage, we do not need to be 100% correct about the text
        // selection data, because it will be updated later from
        // EditorMenuControllerImpl::OnTextAvailable.
        EditorContext(EditorMode::kHardBlocked,
                      EditorTextSelectionMode::kNoSelection,
                      /*consent_status_settled=*/false,
                      /*preset_queries=*/{}));
    return;
  }

  card_session_->editor_manager()->GetEditorPanelContext(
      base::BindOnce(&EditorMenuControllerImpl::OnGetEditorCardMenuContext,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     card_session_->GetLobsterMode()));
}

void EditorMenuControllerImpl::OnGetAnchorBoundsAndEditorContextForTesting(
    const gfx::Rect& anchor_bounds,
    const EditorContext& context) {
  OnGetAnchorBoundsAndEditorContext(anchor_bounds, LobsterMode::kBlocked,
                                    std::move(context));
}

void EditorMenuControllerImpl::OnGetEditorCardMenuContext(
    base::OnceCallback<void(const EditorMenuCardContext&)> callback,
    LobsterMode lobster_mode,
    const EditorContext& editor_context) {
  std::move(callback).Run(
      EditorMenuCardContext()
          .set_consent_status_settled(editor_context.consent_status_settled)
          .set_editor_preset_queries(editor_context.preset_queries)
          .set_editor_mode(editor_context.mode)
          .set_lobster_mode(lobster_mode)
          .set_text_selection_mode(
              editor_context.text_selection_mode ==
                      EditorTextSelectionMode::kHasSelection
                  ? EditorMenuCardTextSelectionMode::kHasSelection
                  : EditorMenuCardTextSelectionMode::kNoSelection)
          .build());
}

void EditorMenuControllerImpl::OnGetAnchorBoundsAndEditorContext(
    const gfx::Rect& anchor_bounds,
    LobsterMode lobster_mode,
    const EditorContext& editor_context) {
  EditorMenuCardContext editor_menu_card_context =
      EditorMenuCardContext()
          .set_consent_status_settled(editor_context.consent_status_settled)
          .set_editor_preset_queries(editor_context.preset_queries)
          .set_editor_mode(editor_context.mode)
          .set_lobster_mode(lobster_mode)
          .set_text_selection_mode(
              editor_context.text_selection_mode ==
                      EditorTextSelectionMode::kHasSelection
                  ? EditorMenuCardTextSelectionMode::kHasSelection
                  : EditorMenuCardTextSelectionMode::kNoSelection)
          .build();

  TextAndImageMode text_and_image_mode =
      editor_menu_card_context.text_and_image_mode();

  switch (text_and_image_mode) {
    case TextAndImageMode::kBlocked:
      break;
    case TextAndImageMode::kPromoCard:
      if (chromeos::features::IsMagicBoostRevampEnabled()) {
        NOTREACHED();
      }
      editor_menu_widget_ = EditorMenuPromoCardView::CreateWidget(
          &application_locale_storage_.get(), anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
    case TextAndImageMode::kEditorWriteOnly:
    case TextAndImageMode::kEditorRewriteOnly:
    case TextAndImageMode::kLobsterWithNoSelectedText:
    case TextAndImageMode::kLobsterWithSelectedText:
    case TextAndImageMode::kEditorWriteAndLobster:
    case TextAndImageMode::kEditorRewriteAndLobster:
      editor_menu_widget_ = EditorMenuView::CreateWidget(
          &application_locale_storage_.get(), text_and_image_mode,
          editor_menu_card_context.preset_queries(), anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
  }

  if (card_session_ == nullptr) {
    return;
  }

  if (card_session_->editor_manager() != nullptr &&
      editor_context.mode != EditorMode::kSoftBlocked &&
      editor_context.mode != EditorMode::kHardBlocked) {
    card_session_->editor_manager()->LogEditorMode(editor_context.mode);
  }

  if (text_and_image_mode == TextAndImageMode::kLobsterWithNoSelectedText ||
      text_and_image_mode == TextAndImageMode::kLobsterWithSelectedText) {
    card_session_->SetActiveFeature(EditorCardSession::ActiveFeature::kLobster);
  } else {
    card_session_->SetActiveFeature(EditorCardSession::ActiveFeature::kEditor);
  }
}

void EditorMenuControllerImpl::OnEditorCardHidden(bool destroy_session) {
  // The currently visible card is closing and being removed from the user's
  // view, the EditorCardSession has ended.
  if (destroy_session && card_session_) {
    card_session_.reset();
  }
}

void EditorMenuControllerImpl::DisableEditorMenu() {
  auto* editor_menu_view = editor_menu_widget_->GetContentsView();
  if (views::IsViewClass<EditorMenuView>(editor_menu_view)) {
    views::AsViewClass<EditorMenuView>(editor_menu_view)->DisableMenu();
  }
}

base::WeakPtr<EditorMenuControllerImpl> EditorMenuControllerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

EditorMenuControllerImpl::EditorCardSession::EditorCardSession(
    EditorMenuControllerImpl* controller,
    std::unique_ptr<EditorManager> editor_manager,
    std::unique_ptr<LobsterManager> lobster_manager)
    : controller_(controller),
      editor_manager_(std::move(editor_manager)),
      lobster_manager_(std::move(lobster_manager)) {
  if (editor_manager_) {
    editor_manager_->AddObserver(this);
  }
}

EditorMenuControllerImpl::EditorCardSession::~EditorCardSession() {
  if (editor_manager_) {
    editor_manager_->RemoveObserver(this);
  }
}

void EditorMenuControllerImpl::EditorCardSession::OnEditorModeChanged(
    EditorMode mode) {
  if (mode == EditorMode::kHardBlocked || mode == EditorMode::kSoftBlocked) {
    controller_->DismissCard();
  }
}

LobsterMode EditorMenuControllerImpl::EditorCardSession::GetLobsterMode()
    const {
  if (!base::FeatureList::IsEnabled(ash::features::kLobsterRightClickMenu) ||
      lobster_manager() == nullptr) {
    return LobsterMode::kBlocked;
  }

  if (selected_text_.size() > 0) {
    return LobsterMode::kSelectedText;
  }

  return LobsterMode::kNoSelectedText;
}

void EditorMenuControllerImpl::EditorCardSession::SetActiveFeature(
    ActiveFeature feature) {
  active_feature = feature;
}

void EditorMenuControllerImpl::EditorCardSession::StartFlowWithFreeformText(
    const std::string& freeform_text) {
  switch (active_feature) {
    case ActiveFeature::kEditor:
      if (editor_manager_) {
        editor_manager_->StartEditingFlowWithFreeform(freeform_text);
      }
      return;
    case ActiveFeature::kLobster:
      if (lobster_manager_) {
        lobster_manager_->StartFlow(freeform_text);
      }
      return;
  }
}

void EditorMenuControllerImpl::EditorCardSession::StartFlowWithPreset(
    const std::string& preset_id) {
  if (preset_id == kLobsterPresetId && lobster_manager_) {
    lobster_manager_->StartFlow(selected_text_);
    return;
  }
  if (editor_manager_) {
    editor_manager_->StartEditingFlowWithPreset(preset_id);
  }
}

void EditorMenuControllerImpl::EditorCardSession::OpenSettings() {
  GURL setting_url;

  switch (active_feature) {
    case ActiveFeature::kEditor:
      setting_url = GURL(base::StrCat(
          {"chrome://os-settings/",
           chromeos::settings::mojom::kSystemPreferencesSectionPath,
           "?settingId=",
           base::NumberToString(static_cast<int>(
               chromeos::settings::mojom::Setting::kShowOrca))}));
      break;
    case ActiveFeature::kLobster:
      setting_url = GURL(base::StrCat(
          {"chrome://os-settings/",
           chromeos::settings::mojom::kSystemPreferencesSectionPath,
           "?settingId=",
           base::NumberToString(static_cast<int>(
               chromeos::settings::mojom::Setting::kLobsterOnOff))}));
      break;
  }

  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      setting_url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void EditorMenuControllerImpl::EditorCardSession::OnSelectedTextChanged(
    const std::string& text) {
  selected_text_ = text;
}

}  // namespace chromeos::editor_menu
