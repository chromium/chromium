// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"

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

AiIntroScreenHandler::AiIntroScreenHandler() : BaseScreenHandler(kScreenId) {}

AiIntroScreenHandler::~AiIntroScreenHandler() = default;

// Add localized values that you want to propagate to the JS side here.
void AiIntroScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("aiIntroScreenTitle", IDS_AI_INTRO_TITLE);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (base::FeatureList::IsEnabled(features::kFeatureManagementOobeAiIntro)) {
    auto product_name =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_CROS_OOBE_PRODUCT_NAME);
    product_name = base::TrimWhitespaceASCII(product_name, base::TRIM_TRAILING);
    builder->AddF("aiIntroScreenSubtitle", IDS_AI_INTRO_SUBTITLE,
                  base::UTF8ToUTF16(product_name));
  } else
#endif
  {
    builder->AddF("aiIntroScreenSubtitle", IDS_AI_INTRO_SUBTITLE,
                  ui::GetChromeOSDeviceName());
  }
  builder->Add("slideWriteTitle", IDS_AI_INTRO_SLIDE_WRITE_TITLE);
  builder->Add("slideWriteText", IDS_AI_INTRO_SLIDE_WRITE_TEXT);
  builder->Add("slideWallpaperTitle", IDS_AI_INTRO_SLIDE_WALLPAPER_TITLE);
  builder->Add("slideWallpaperText", IDS_AI_INTRO_SLIDE_WALLPAPER_TEXT);
  builder->Add("slideBackgroundTitle", IDS_AI_INTRO_SLIDE_BACKGROUND_TITLE);
  builder->Add("slideBackgroundText", IDS_AI_INTRO_SLIDE_BACKGROUND_TEXT);
  builder->Add("slideLabel", IDS_AI_INTRO_SLIDE_LABEL);
  builder->Add("slideSelectedButtonLabel", IDS_AI_INTRO_SELECTED_BUTTON_LABEL);
  builder->Add("slideUnselectedButtonLabel",
               IDS_AI_INTRO_UNSELECTED_BUTTON_LABEL);
}

void AiIntroScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<AiIntroScreenView> AiIntroScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
