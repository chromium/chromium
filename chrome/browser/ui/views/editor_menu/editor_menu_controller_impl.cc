// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"

#include <string_view>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_promo_card_view.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos::editor_menu {

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

using ash::input_method::EditorMediator;
using ash::input_method::EditorPanelManager;
using ash::input_method::EditorPanelMode;

EditorPanelManager& GetEditorPanelManager() {
  CHECK(EditorMediator::Get());
  return EditorMediator::Get()->panel_manager();
}

}  // namespace
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

EditorMenuControllerImpl::EditorMenuControllerImpl() = default;

EditorMenuControllerImpl::~EditorMenuControllerImpl() = default;

void EditorMenuControllerImpl::MaybeShowEditorMenu(
    const gfx::Rect& anchor_bounds) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetEditorPanelManager().GetEditorPanelContext(
      base::BindOnce(&EditorMenuControllerImpl::OnGetEditorPanelContextResult,
                     weak_factory_.GetWeakPtr(), anchor_bounds));
#else
  // TODO(b/295059934): Implement crosapi to call EditorPanelManager before
  // showing views in lacros.
  editor_menu_widget_ =
      EditorMenuPromoCardView::CreateWidget(anchor_bounds, this);
  editor_menu_widget_->ShowInactive();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void EditorMenuControllerImpl::DismissEditorMenu() {
  if (editor_menu_widget_) {
    editor_menu_widget_.reset();
  }
}

void EditorMenuControllerImpl::UpdateAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  if (!editor_menu_widget_) {
    return;
  }

  // Update the bounds of the shown view.
  // TODO(b/295060733): The main view.
  // TODO(b/295061567): The consent view.
}

void EditorMenuControllerImpl::OnSettingsButtonPressed() {}

void EditorMenuControllerImpl::OnChipButtonPressed(int button_id,
                                                   const std::u16string& text) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetEditorPanelManager().StartEditingFlowWithPreset("");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void EditorMenuControllerImpl::OnTextfieldArrowButtonPressed(
    const std::u16string& text) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetEditorPanelManager().StartEditingFlowWithFreeform(base::UTF16ToUTF8(text));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void EditorMenuControllerImpl::OnPromoCardDismissButtonPressed() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetEditorPanelManager().OnConsentDeclined();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void EditorMenuControllerImpl::OnPromoCardTellMeMoreButtonPressed() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GetEditorPanelManager().StartEditingFlow();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void EditorMenuControllerImpl::OnGetEditorPanelContextResult(
    gfx::Rect anchor_bounds,
    const EditorPanelContext& context) {
  switch (context.editor_panel_mode) {
    case EditorPanelMode::kBlocked:
      break;
    case EditorPanelMode::kWrite:
    case EditorPanelMode::kRewrite:
      editor_menu_widget_ = EditorMenuView::CreateWidget(anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
    case EditorPanelMode::kPromoCard:
      editor_menu_widget_ =
          EditorMenuPromoCardView::CreateWidget(anchor_bounds, this);
      editor_menu_widget_->ShowInactive();
      break;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace chromeos::editor_menu
