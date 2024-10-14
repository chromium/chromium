// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"

#include <memory>
#include <string_view>
#include <vector>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/editor_menu/editor_manager_factory.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/editor_types.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "ash/public/cpp/new_window_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::editor_menu {

EditorMenuControllerImpl::EditorMenuControllerImpl() = default;

EditorMenuControllerImpl::~EditorMenuControllerImpl() = default;

void EditorMenuControllerImpl::OnContextMenuShown(Profile* profile) {}

void EditorMenuControllerImpl::OnTextAvailable(
    const gfx::Rect& anchor_bounds,
    const std::string& selected_text,
    const std::string& surrounding_text) {
  if (!card_session_) {
    return;
  }

  card_session_->manager().GetEditorPanelContext(base::BindOnce(
      &EditorMenuControllerImpl::OnGetAnchorBoundsAndEditorContext,
      weak_factory_.GetWeakPtr(), anchor_bounds));
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
    editor_menu_widget_.reset();
  }
}

void EditorMenuControllerImpl::OnSettingsButtonPressed() {
  GURL setting_url = GURL(base::StrCat(
      {"chrome://os-settings/",
       chromeos::MagicBoostState::Get() &&
               chromeos::MagicBoostState::Get()->IsMagicBoostAvailable()
           ? chromeos::settings::mojom::kSystemPreferencesSectionPath
           : chromeos::settings::mojom::kInputSubpagePath,
       "?settingId=",
       base::NumberToString(
           static_cast<int>(chromeos::settings::mojom::Setting::kShowOrca))}));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  DCHECK(service->IsAvailable<crosapi::mojom::UrlHandler>());

  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(setting_url);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      setting_url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#endif
}

void EditorMenuControllerImpl::OnChipButtonPressed(
    std::string_view text_query_id) {
  if (!card_session_) {
    return;
  }

  DisableEditorMenu();
  card_session_->manager().StartEditingFlowWithPreset(
      std::string(text_query_id));
}

void EditorMenuControllerImpl::OnTextfieldArrowButtonPressed(
    std::u16string_view text) {
  if (text.empty() || !card_session_) {
    return;
  }

  DisableEditorMenu();
  card_session_->manager().StartEditingFlowWithFreeform(
      base::UTF16ToUTF8(text));
}

void EditorMenuControllerImpl::OnPromoCardWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  if (!card_session_) {
    return;
  }

  switch (closed_reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      card_session_->manager().StartEditingFlow();
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      card_session_->manager().OnPromoCardDeclined();
      break;
    default:
      card_session_->manager().OnPromoCardDismissed();
      break;
  }

  OnEditorCardHidden();
}

void EditorMenuControllerImpl::OnEditorMenuVisibilityChanged(bool visible) {
  if (!card_session_) {
    return;
  }

  card_session_->manager().OnEditorMenuVisibilityChanged(visible);

  if (!visible) {
    OnEditorCardHidden();
  }
}

bool EditorMenuControllerImpl::SetBrowserContext(
    content::BrowserContext* context) {
  std::unique_ptr<EditorManager> editor_manager = CreateEditorManager(context);

  if (editor_manager) {
    card_session_ =
        std::make_unique<EditorCardSession>(this, std::move(editor_manager));
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
  if (!card_session_) {
    return;
  }
  card_session_->manager().RequestCacheContext();
}

void EditorMenuControllerImpl::LogEditorMode(const EditorMode& editor_mode) {
  if (!card_session_) {
    return;
  }
  card_session_->manager().LogEditorMode(editor_mode);
}

void EditorMenuControllerImpl::GetEditorContext(
    base::OnceCallback<void(const EditorContext&)> callback) {
  if (!card_session_) {
    return;
  }
  card_session_->manager().GetEditorPanelContext(
      base::BindOnce(&EditorMenuControllerImpl::OnGetEditorContext,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void EditorMenuControllerImpl::OnGetAnchorBoundsAndEditorContextForTesting(
    const gfx::Rect& anchor_bounds,
    const EditorContext& context) {
  OnGetAnchorBoundsAndEditorContext(anchor_bounds, std::move(context));
}

void EditorMenuControllerImpl::OnGetEditorContext(
    base::OnceCallback<void(const EditorContext&)> callback,
    const EditorContext& context) {
  std::move(callback).Run(context);
}

void EditorMenuControllerImpl::OnGetAnchorBoundsAndEditorContext(
    const gfx::Rect& anchor_bounds,
    const EditorContext& context) {
  switch (context.mode) {
    case EditorMode::kHardBlocked:
    case EditorMode::kSoftBlocked:
      break;
    case EditorMode::kWrite:
      editor_menu_widget_ = EditorMenuView::CreateWidget(
          EditorMenuMode::kWrite, PresetTextQueries(), anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
    case EditorMode::kRewrite:
      editor_menu_widget_ = EditorMenuView::CreateWidget(
          EditorMenuMode::kRewrite, context.preset_queries, anchor_bounds,
          this);
      editor_menu_widget_->ShowInactive();
      break;
    case EditorMode::kPromoCard:
      editor_menu_widget_ =
          EditorMenuPromoCardView::CreateWidget(anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
  }
  if (card_session_ != nullptr && context.mode != EditorMode::kSoftBlocked &&
      context.mode != EditorMode::kHardBlocked) {
    card_session_->manager().LogEditorMode(context.mode);
  }
}

void EditorMenuControllerImpl::OnEditorCardHidden() {
  // The currently visible card is closing and being removed from the user's
  // view, the EditorCardSession has ended.
  if (card_session_) {
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
    std::unique_ptr<EditorManager> editor_manager)
    : controller_(controller), manager_(std::move(editor_manager)) {
  manager_->AddObserver(this);
}

EditorMenuControllerImpl::EditorCardSession::~EditorCardSession() {
  manager_->RemoveObserver(this);
}

void EditorMenuControllerImpl::EditorCardSession::OnEditorModeChanged(
    const EditorMode& mode) {
  if (mode == EditorMode::kHardBlocked || mode == EditorMode::kSoftBlocked) {
    controller_->DismissCard();
  }
}

EditorManager& EditorMenuControllerImpl::EditorCardSession::manager() {
  return *manager_;
}

}  // namespace chromeos::editor_menu
