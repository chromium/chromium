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
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "content/public/browser/browser_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#else
#include "ash/public/cpp/new_window_delegate.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::editor_menu {

namespace {

using crosapi::mojom::EditorPanelMode;
using crosapi::mojom::EditorPanelPresetTextQuery;
using crosapi::mojom::EditorPanelPresetTextQueryPtr;

crosapi::mojom::EditorPanelManager* GetEditorPanelManager(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* const lacros_service =
      chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::EditorPanelManager>());
  return (lacros_service->GetRemote<crosapi::mojom::EditorPanelManager>()
              .is_bound() &&
          lacros_service->GetRemote<crosapi::mojom::EditorPanelManager>()
              .is_connected())
             ? lacros_service->GetRemote<crosapi::mojom::EditorPanelManager>()
                   .get()
             : nullptr;
#else
  ash::input_method::EditorMediator* editor_mediator =
      ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(browser_context));
  return editor_mediator ? editor_mediator->panel_manager() : nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

PresetQueryCategory GetPresetQueryCategory(
    const crosapi::mojom::EditorPanelPresetQueryCategory category) {
  switch (category) {
    case crosapi::mojom::EditorPanelPresetQueryCategory::kUnknown:
      return PresetQueryCategory::kUnknown;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kShorten:
      return PresetQueryCategory::kShorten;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kElaborate:
      return PresetQueryCategory::kElaborate;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kRephrase:
      return PresetQueryCategory::kRephrase;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kFormalize:
      return PresetQueryCategory::kFormalize;
    case crosapi::mojom::EditorPanelPresetQueryCategory::kEmojify:
      return PresetQueryCategory::kEmojify;
  }
}

PresetTextQueries GetPresetTextQueries(
    const std::vector<EditorPanelPresetTextQueryPtr>& preset_text_queries) {
  PresetTextQueries queries;
  for (const auto& query : preset_text_queries) {
    queries.emplace_back(query->text_query_id, base::UTF8ToUTF16(query->name),
                         GetPresetQueryCategory(query->category));
  }
  return queries;
}

}  // namespace

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

  card_session_->panel_manager.GetEditorPanelContext(
      base::BindOnce(&EditorMenuControllerImpl::OnGetEditorPanelContextResult,
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
  GURL setting_url = GURL(base::StrCat({"chrome://os-settings/",
                    chromeos::settings::mojom::kInputSubpagePath, "?settingId=",
                    base::NumberToString(static_cast<int>(
                        chromeos::settings::mojom::Setting::kShowOrca))}));
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
  card_session_->panel_manager.StartEditingFlowWithPreset(
      std::string(text_query_id));
}

void EditorMenuControllerImpl::OnTextfieldArrowButtonPressed(
    std::u16string_view text) {
  if (text.empty() || !card_session_) {
    return;
  }

  DisableEditorMenu();
  card_session_->panel_manager.StartEditingFlowWithFreeform(
      base::UTF16ToUTF8(text));
}

void EditorMenuControllerImpl::OnPromoCardWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  if (!card_session_) {
    return;
  }

  switch (closed_reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      card_session_->panel_manager.StartEditingFlow();
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      card_session_->panel_manager.OnPromoCardDeclined();
      break;
    default:
      card_session_->panel_manager.OnPromoCardDismissed();
      break;
  }

  OnEditorCardHidden();
}

void EditorMenuControllerImpl::OnEditorMenuVisibilityChanged(bool visible) {
  if (!card_session_) {
    return;
  }

  card_session_->panel_manager.OnEditorMenuVisibilityChanged(visible);

  if (!visible) {
    OnEditorCardHidden();
  }
}

void EditorMenuControllerImpl::SetBrowserContext(
    content::BrowserContext* context) {
  if (auto* panel_manager = GetEditorPanelManager(context);
      panel_manager != nullptr) {
    // A card session can only be initialized if we are able to connect
    // successfully to the backend panel manager.
    card_session_ = std::make_unique<EditorCardSession>(*panel_manager);
  }
}

void EditorMenuControllerImpl::OnGetEditorPanelContextResultForTesting(
    const gfx::Rect& anchor_bounds,
    crosapi::mojom::EditorPanelContextPtr context) {
  OnGetEditorPanelContextResult(anchor_bounds, std::move(context));
}

void EditorMenuControllerImpl::OnGetEditorPanelContextResult(
    const gfx::Rect& anchor_bounds,
    crosapi::mojom::EditorPanelContextPtr context) {
  switch (context->editor_panel_mode) {
    case EditorPanelMode::kBlocked:
      break;
    case EditorPanelMode::kWrite:
      editor_menu_widget_ = EditorMenuView::CreateWidget(
          EditorMenuMode::kWrite, PresetTextQueries(), anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
    case EditorPanelMode::kRewrite:
      editor_menu_widget_ = EditorMenuView::CreateWidget(
          EditorMenuMode::kRewrite,
          GetPresetTextQueries(context->preset_text_queries), anchor_bounds,
          this);
      editor_menu_widget_->ShowInactive();
      break;
    case EditorPanelMode::kPromoCard:
      editor_menu_widget_ =
          EditorMenuPromoCardView::CreateWidget(anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
  }
  if (card_session_ != nullptr) {
    card_session_->panel_manager.LogEditorMode(context->editor_panel_mode);
  }
}

void EditorMenuControllerImpl::OnEditorCardHidden() {
  // The currently visible card is closing and being removed from the user's
  // view, the EditorCardSession has ended.
  card_session_ = nullptr;
}

void EditorMenuControllerImpl::DisableEditorMenu() {
  auto* editor_menu_view = editor_menu_widget_->GetContentsView();
  if (views::IsViewClass<EditorMenuView>(editor_menu_view)) {
    views::AsViewClass<EditorMenuView>(editor_menu_view)->DisableMenu();
  }
}

}  // namespace chromeos::editor_menu
