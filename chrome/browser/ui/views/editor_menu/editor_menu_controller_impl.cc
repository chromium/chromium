// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"

#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/preset_text_query.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/input_method/editor_mediator.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::editor_menu {

namespace {

using crosapi::mojom::EditorPanelMode;
using crosapi::mojom::EditorPanelPresetTextQuery;
using crosapi::mojom::EditorPanelPresetTextQueryPtr;

crosapi::mojom::EditorPanelManager& GetEditorPanelManager() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* const lacros_service =
      chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::EditorPanelManager>());
  return *lacros_service->GetRemote<crosapi::mojom::EditorPanelManager>().get();
#else
  CHECK(ash::input_method::EditorMediator::Get());
  return ash::input_method::EditorMediator::Get()->panel_manager();
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

void EditorMenuControllerImpl::OnContextMenuShown() {}

void EditorMenuControllerImpl::OnTextAvailable(
    const gfx::Rect& anchor_bounds,
    const std::string& selected_text,
    const std::string& surrounding_text) {
  GetEditorPanelManager().GetEditorPanelContext(
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

void EditorMenuControllerImpl::OnSettingsButtonPressed() {}

void EditorMenuControllerImpl::OnChipButtonPressed(
    std::string_view text_query_id) {
  GetEditorPanelManager().StartEditingFlowWithPreset(
      std::string(text_query_id));
}

void EditorMenuControllerImpl::OnTextfieldArrowButtonPressed(
    std::u16string_view text) {
  GetEditorPanelManager().StartEditingFlowWithFreeform(base::UTF16ToUTF8(text));
}

void EditorMenuControllerImpl::OnPromoCardWidgetClosed(
    views::Widget::ClosedReason closed_reason) {
  switch (closed_reason) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      GetEditorPanelManager().StartEditingFlow();
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      GetEditorPanelManager().OnPromoCardDeclined();
      break;
    default:
      GetEditorPanelManager().OnPromoCardDismissed();
      break;
  }
}

void EditorMenuControllerImpl::OnEditorMenuVisibilityChanged(bool visible) {
  GetEditorPanelManager().OnEditorMenuVisibilityChanged(visible);
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
    case EditorPanelMode::kRewrite:
      editor_menu_widget_ = EditorMenuView::CreateWidget(
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
}

}  // namespace chromeos::editor_menu
