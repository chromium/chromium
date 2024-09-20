// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/access_code_cast_resources.h"
#include "chrome/grit/access_code_cast_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace media_router {

bool AccessCodeCastUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return base::FeatureList::IsEnabled(features::kAccessCodeCastUI) &&
         media_router::GetAccessCodeCastEnabledPref(profile);
}

AccessCodeCastUI::AccessCodeCastUI(content::WebUI* web_ui)
    : MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIAccessCodeCastHost);
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kAccessCodeCastResources, kAccessCodeCastResourcesSize),
      IDR_ACCESS_CODE_CAST_INDEX_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"accessCodeMessage", IDS_ACCESS_CODE_CAST_ACCESS_CODE_MESSAGE},
      {"back", IDS_ACCESS_CODE_CAST_BACK},
      {"cancel", IDS_CANCEL},
      {"cast", IDS_ACCESS_CODE_CAST_CAST},
      {"dialogTitle", IDS_ACCESS_CODE_CAST_DIALOG_TITLE},
      {"enterCharacter", IDS_ACCESS_CODE_CAST_ENTER_CHARACTER},
      {"errorAccessCode", IDS_ACCESS_CODE_CAST_ERROR_ACCESS_CODE},
      {"errorDifferentNetwork", IDS_ACCESS_CODE_CAST_ERROR_DIFFERENT_NETWORK},
      {"errorNetwork", IDS_ACCESS_CODE_CAST_ERROR_NETWORK},
      {"errorPermission", IDS_ACCESS_CODE_CAST_ERROR_PERMISSION},
      {"errorProfileSync", IDS_ACCESS_CODE_CAST_ERROR_PROFILE_SYNC},
      {"errorTooManyRequests", IDS_ACCESS_CODE_CAST_ERROR_TOO_MANY_REQUESTS},
      {"errorUnknown", IDS_ACCESS_CODE_CAST_ERROR_UNKNOWN},
      {"inputLabel", IDS_ACCESS_CODE_CAST_INPUT_ARIA_LABEL},
      {"learnMore", IDS_LEARN_MORE},
      {"submit", IDS_ACCESS_CODE_CAST_SUBMIT},
      {"useCamera", IDS_ACCESS_CODE_CAST_USE_CAMERA},
  };

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("qrScannerEnabled", false);
  source->AddString("learnMoreUrl", chrome::kAccessCodeCastLearnMoreURL);

  Profile* const profile = Profile::FromWebUI(web_ui);
  source->AddInteger("rememberedDeviceDuration",
                     GetAccessCodeDeviceDurationPref(profile).InSeconds());

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "managedFootnoteHours", IDS_ACCESS_CODE_CAST_MANAGED_FOOTNOTE_HOURS);
  plural_string_handler->AddLocalizedString(
      "managedFootnoteDays", IDS_ACCESS_CODE_CAST_MANAGED_FOOTNOTE_DAYS);
  plural_string_handler->AddLocalizedString(
      "managedFootnoteMonths", IDS_ACCESS_CODE_CAST_MANAGED_FOOTNOTE_MONTHS);
  plural_string_handler->AddLocalizedString(
      "managedFootnoteYears", IDS_ACCESS_CODE_CAST_MANAGED_FOOTNOTE_YEARS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

AccessCodeCastUI::~AccessCodeCastUI() = default;

void AccessCodeCastUI::SetCastModeSet(const CastModeSet& cast_mode_set) {
  cast_mode_set_ = cast_mode_set;
}

void AccessCodeCastUI::SetDialogCreationTimestamp(
    base::Time dialog_creation_timestamp) {
  dialog_creation_timestamp_ = dialog_creation_timestamp;
}

void AccessCodeCastUI::SetMediaRouteStarter(
    std::unique_ptr<MediaRouteStarter> media_route_starter) {
  media_route_starter_ = std::move(media_route_starter);
}

void AccessCodeCastUI::BindInterface(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandlerFactory>
        receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void AccessCodeCastUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void AccessCodeCastUI::CreatePageHandler(
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> receiver) {
  DCHECK(page);

  if (dialog_creation_timestamp_.has_value()) {
    base::TimeDelta dialog_load_time =
        base::Time::Now() - (dialog_creation_timestamp_.value());
    AccessCodeCastMetrics::RecordDialogLoadTime(dialog_load_time);
  }

  page_handler_ = std::make_unique<AccessCodeCastHandler>(
      std::move(receiver), std::move(page), cast_mode_set_,
      std::move(media_route_starter_));
}

WEB_UI_CONTROLLER_TYPE_IMPL(AccessCodeCastUI)

}  // namespace media_router
