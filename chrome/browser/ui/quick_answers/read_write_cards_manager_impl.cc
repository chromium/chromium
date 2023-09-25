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
#include "content/public/browser/context_menu_params.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"

namespace chromeos {

namespace {

constexpr base::StringPiece kOrcaKey = "orca-key";
constexpr char kOrcaKeyHash[] =
    "\x7a\xf3\xa1\x57\x28\x48\xc4\x14\x27\x13\x53\x5a\x09\xf3\x0e\xfc\xee\xa6"
    "\xbb\xa4";

bool CheckOrcaKey() {
  const std::string& debug_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          /*ash::switches::kOrcaKey=*/kOrcaKey));
  // See go/orca-key for the key.
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/auuf123 --orca-key="INSERT KEY
  //  HERE" --enable-features=Orca
  bool orca_key_check = (debug_key_hash == kOrcaKeyHash);
  if (!orca_key_check) {
    LOG(ERROR) << "Provided debug key does not match with the expected one.";
  }
  return orca_key_check;
}

}  // namespace

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
    const content::ContextMenuParams& params) {
  // Skip password input field.
  const bool is_password_field =
      params.input_field_type ==
      blink::mojom::ContextMenuDataInputFieldType::kPassword;
  if (is_password_field) {
    return nullptr;
  }

  if (chromeos::features::IsOrcaEnabled()) {
    if (params.is_editable) {
      return CheckOrcaKey() ? editor_menu_controller_.get() : nullptr;
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
