// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/wrong_hwid_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

WrongHWIDScreenHandler::WrongHWIDScreenHandler()
    : BaseScreenHandler(kScreenId) {}

WrongHWIDScreenHandler::~WrongHWIDScreenHandler() = default;

void WrongHWIDScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<WrongHWIDScreenView> WrongHWIDScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WrongHWIDScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("wrongHWIDScreenHeader", IDS_WRONG_HWID_SCREEN_HEADER);
  builder->Add("wrongHWIDMessageFirstPart",
                IDS_WRONG_HWID_SCREEN_MESSAGE_FIRST_PART);
  builder->Add("wrongHWIDMessageSecondPart",
                IDS_WRONG_HWID_SCREEN_MESSAGE_SECOND_PART);
  builder->Add("wrongHWIDScreenSkipLink",
                IDS_WRONG_HWID_SCREEN_SKIP_LINK);
}

}  // namespace ash
