// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

GeminiIntroScreenHandler::GeminiIntroScreenHandler() :
    BaseScreenHandler(kScreenId) {}

GeminiIntroScreenHandler::~GeminiIntroScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void GeminiIntroScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (base::FeatureList::IsEnabled(
      features::kFeatureManagementOobeGeminiIntro)) {
    auto product_name =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_CROS_OOBE_PRODUCT_NAME);
    product_name = base::TrimWhitespaceASCII(product_name, base::TRIM_TRAILING);
    builder->AddF("geminiIntroScreenTitle", IDS_GEMINI_INTRO_TITLE,
                  base::UTF8ToUTF16(product_name));
  } else
#endif
  {
    builder->AddF("geminiIntroScreenTitle", IDS_GEMINI_INTRO_TITLE,
                  ui::GetChromeOSDeviceName());
  }
  builder->Add("geminiIntroScreenSubtitle", IDS_GEMINI_INTRO_SUBTITLE);
}

void GeminiIntroScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

base::WeakPtr<GeminiIntroScreenView> GeminiIntroScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
