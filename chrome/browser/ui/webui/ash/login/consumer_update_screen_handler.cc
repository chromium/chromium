// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"

#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

ConsumerUpdateScreenHandler::ConsumerUpdateScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ConsumerUpdateScreenHandler::~ConsumerUpdateScreenHandler() = default;

void ConsumerUpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("consumerUpdateScreenAcceptButton",
               IDS_CONSUMER_UPDATE_ACCEPT_BUTTON);
  builder->Add("consumerUpdateScreenSkipButton",
               IDS_CONSUMER_UPDATE_SKIP_BUTTON);
  builder->Add("consumerUpdateScreenCellularTitle",
               IDS_CONSUMER_UPDATE_CELLULAR_TITLE);
  builder->Add("consumerUpdateScreenInProgressTitle",
               IDS_CONSUMER_UPDATE_PROGRESS_TITLE);
  builder->Add("consumerUpdateScreenInProgressSubtitle",
               IDS_CONSUMER_UPDATE_PROGRESS_SUBTITLE);
  builder->Add("consumerUpdateScreenInProgressAdditionalSubtitle",
               IDS_CONSUMER_UPDATE_PROGRESS_ADDITIONAL_SUBTITLE);
}

void ConsumerUpdateScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<ConsumerUpdateScreenView>
ConsumerUpdateScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
