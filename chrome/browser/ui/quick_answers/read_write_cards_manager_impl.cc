// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/read_write_cards_manager_impl.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"

namespace chromeos {

ReadWriteCardsManagerImpl::ReadWriteCardsManagerImpl()
    : quick_answers_controller_(
          std::make_unique<QuickAnswersControllerImpl>()) {
  quick_answers_controller_->SetClient(
      std::make_unique<quick_answers::QuickAnswersClient>(
          g_browser_process->shared_url_loader_factory(),
          quick_answers_controller_->GetQuickAnswersDelegate()));

  if (chromeos::features::IsOrcaEnabled()) {
    editor_menu_controller_ =
        std::make_unique<editor_menu::EditorMenuControllerImpl>();
  }
}

ReadWriteCardsManagerImpl::~ReadWriteCardsManagerImpl() = default;

ReadWriteCardController* ReadWriteCardsManagerImpl::GetController(
    const content::ContextMenuParams& params,
    content::BrowserContext* context) {
  // Skip password input field.
  const bool is_password_field =
      params.form_control_type == blink::mojom::FormControlType::kInputPassword;
  if (is_password_field) {
    return nullptr;
  }

  if (chromeos::features::IsOrcaEnabled()) {
    if (params.is_editable) {
      editor_menu_controller_->SetBrowserContext(context);
      return editor_menu_controller_.get();
    }
  }

  if (!QuickAnswersState::Get()->is_eligible()) {
    return nullptr;
  }

  // Skip if no text selected.
  if (params.selection_text.empty()) {
    return nullptr;
  }

  return quick_answers_controller_.get();
}

}  // namespace chromeos
