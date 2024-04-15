// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/tuna_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

TunaScreenHandler::TunaScreenHandler() : BaseScreenHandler(kScreenId) {}

TunaScreenHandler::~TunaScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void TunaScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(b/334819371) Cleanup strings
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (base::FeatureList::IsEnabled(features::kFeatureManagementOobeTuna)) {
    auto product_name =
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_CROS_OOBE_PRODUCT_NAME);
    // Trim trailing space
    product_name = product_name.substr(0, product_name.length() - 1);
    builder->AddF("tunaScreenTitle", IDS_TUNA_TITLE,
                  base::UTF8ToUTF16(product_name));
  } else
#endif
  {
    builder->AddF("tunaScreenTitle", IDS_TUNA_TITLE,
                  ui::GetChromeOSDeviceName());
  }
  builder->Add("tunaScreenSubtitle", IDS_TUNA_SUBTITLE);
}

void TunaScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

base::WeakPtr<TunaScreenView> TunaScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
