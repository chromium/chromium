// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/hardware_data_collection_screen_handler.h"

#include <string>

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/hardware_data_collection_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

namespace {

constexpr char kHardwareUsageEnabled[] = "hwDataUsageEnabled";

}  // namespace

HWDataCollectionScreenHandler::HWDataCollectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

HWDataCollectionScreenHandler::~HWDataCollectionScreenHandler() = default;

void HWDataCollectionScreenHandler::Show(bool enabled) {
  base::Value::Dict data;
  data.Set(kHardwareUsageEnabled, enabled);

  ShowInWebUI(std::move(data));
}

base::WeakPtr<HWDataCollectionView> HWDataCollectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HWDataCollectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("HWDataCollectionTitle", IDS_HW_DATA_COLLECTION_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("HWDataCollectionContent", IDS_HW_DATA_COLLECTION_CONTENT,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("HWDataCollectionUsageInfo", IDS_HW_DATA_COLLECTION_USAGE_INFO,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("HWDataCollectionNextButton",
               IDS_HW_DATA_COLLECTION_ACCEPT_AND_CONTINUE_BUTTON_TEXT);
}

}  // namespace ash
