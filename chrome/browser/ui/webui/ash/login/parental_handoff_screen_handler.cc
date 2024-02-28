// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/parental_handoff_screen_handler.h"

#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kUsername[] = "username";

}  // namespace

ParentalHandoffScreenHandler::ParentalHandoffScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ParentalHandoffScreenHandler::~ParentalHandoffScreenHandler() = default;

void ParentalHandoffScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("parentalHandoffDialogNextButton",
               IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_NEXT_BUTTON);
  builder->Add("parentalHandoffDialogTitle",
               IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_TITLE);
  builder->Add("parentalHandoffDialogSubtitle",
               IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_SUBTITLE);
}

void ParentalHandoffScreenHandler::Show(const std::u16string& username) {
  base::Value::Dict data;
  data.Set(kUsername, username);

  ShowInWebUI(std::move(data));
}

base::WeakPtr<ParentalHandoffScreenView>
ParentalHandoffScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
