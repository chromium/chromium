// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_ui.h"

#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/feedback/report_unsafe_site/report_unsafe_site_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/feedback_resources.h"
#include "chrome/grit/feedback_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/key_value_pair_viewer_shared_resources.h"
#include "chrome/grit/key_value_pair_viewer_shared_resources_map.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/arc/arc_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace feedback_mojom = feedback::report_unsafe_site::mojom;

void AddStringResources(content::WebUIDataSource* source,
                        const Profile* profile) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"additionalInfo", IDS_FEEDBACK_ADDITIONAL_INFO_LABEL},
      {"anonymousUser", IDS_FEEDBACK_ANONYMOUS_EMAIL_OPTION},
      {"attachFileLabel", IDS_FEEDBACK_ATTACH_FILE_LABEL},
      {"attachFileNote", IDS_FEEDBACK_ATTACH_FILE_NOTE},
      {"attachFileToBig", IDS_FEEDBACK_ATTACH_FILE_TO_BIG},
      {"autofillMetadataPageTitle", IDS_FEEDBACK_AUTOFILL_METADATA_PAGE_TITLE},
      {"autofillMetadataInfo", IDS_FEEDBACK_INCLUDE_AUTOFILL_METADATA_CHECKBOX},
      {"cancel", IDS_CANCEL},
      {"consentCheckboxLabel", IDS_FEEDBACK_CONSENT_CHECKBOX_LABEL},
      {"freeFormText", IDS_FEEDBACK_FREE_TEXT_LABEL},
      {"freeFormTextAi", IDS_FEEDBACK_FREE_TEXT_AI_LABEL},
      {"appTitle", IDS_FEEDBACK_REPORT_APP_TITLE},
      {"logIdCheckboxLabel", IDS_FEEDBACK_LOG_ID_CHECKBOX_LABEL},
      {"collapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL},
      {"expandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL},
      {"tableTitle", IDS_ABOUT_SYS_TABLE_TITLE},
      {"noDescription", IDS_FEEDBACK_NO_DESCRIPTION},
      {"offensiveCheckboxLabel", IDS_FEEDBACK_OFFENSIVE_CHECKBOX_LABEL},
      {"pageTitle", IDS_FEEDBACK_REPORT_PAGE_TITLE},
      {"pageUrl", IDS_FEEDBACK_REPORT_URL_LABEL},
      {"privacyNote", IDS_FEEDBACK_PRIVACY_NOTE},
      {"reportUnsafeSiteDialogDescription",
       IDS_REPORT_UNSAFE_SITE_DIALOG_DESCRIPTION},
      {"reportUnsafeSiteDialogFooter", IDS_REPORT_UNSAFE_SITE_DIALOG_FOOTER},
      {"reportUnsafeSiteDialogIncludeScreenshotCheckboxLabel",
       IDS_REPORT_UNSAFE_SITE_DIALOG_INCLUDE_SCREENSHOT_CHECKBOX_LABEL},
      {"reportUnsafeSiteDialogSendButtonLabel",
       IDS_REPORT_UNSAFE_SITE_DIALOG_SEND_BUTTON_LABEL},
      {"reportUnsafeSiteDialogTitle", IDS_REPORT_UNSAFE_SITE_DIALOG_TITLE},
      {"reportUnsafeSiteDialogUrlLabel",
       IDS_REPORT_UNSAFE_SITE_DIALOG_URL_LABEL},
      {"screenshot", IDS_FEEDBACK_SCREENSHOT_LABEL},
      {"screenshotA11y", IDS_FEEDBACK_SCREENSHOT_A11Y_TEXT},
      {"sendReport", IDS_FEEDBACK_SEND_REPORT},
      {"sysinfoPageDescription", IDS_ABOUT_SYS_DESC},
      {"sysinfoPageTitle", IDS_FEEDBACK_SYSINFO_PAGE_TITLE},
      {"userEmail", IDS_FEEDBACK_USER_EMAIL_LABEL},
  };

  source->AddLocalizedStrings(kStrings);
#if BUILDFLAG(IS_CHROMEOS)
  source->AddLocalizedString("mayBeSharedWithPartnerNote",
                             IDS_FEEDBACK_TOOL_MAY_BE_SHARED_NOTE);
  source->AddLocalizedString(
      "sysInfo",
      arc::IsArcPlayStoreEnabledForProfile(profile)
          ? IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX_ARC
          : IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX);
#else
  source->AddLocalizedString("sysInfo",
                             IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_CHKBOX);
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (feedback::ReportUnsafeSiteDialog::IsEnabled(*profile)) {
    source->AddResourcePath("report-unsafe-site",
                            IDR_FEEDBACK_REPORT_UNSAFE_SITE_HTML);
  }
}

void CreateAndAddFeedbackHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIFeedbackHost);
  webui::SetupWebUIDataSource(source, kFeedbackResources,
                              IDR_FEEDBACK_FEEDBACK_HTML);
  source->AddResourcePaths(kKeyValuePairViewerSharedResources);
  AddStringResources(source, profile);
}

FeedbackUI::FeedbackUI(content::WebUI* web_ui) : MojoWebDialogUI(web_ui) {
  CreateAndAddFeedbackHTMLSource(Profile::FromWebUI(web_ui));
}

FeedbackUI::~FeedbackUI() = default;

bool FeedbackUI::IsFeedbackEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed);
}

void FeedbackUI::BindInterface(
    mojo::PendingReceiver<feedback_mojom::PageHandlerFactory> receiver) {
  if (report_unsafe_site_factory_receiver_.is_bound()) {
    report_unsafe_site_factory_receiver_.reset();
  }
  report_unsafe_site_factory_receiver_.Bind(std::move(receiver));
}

void FeedbackUI::CreatePageHandler(
    mojo::PendingReceiver<feedback_mojom::PageHandler> handler) {
  report_unsafe_site_page_handler_ =
      std::make_unique<ReportUnsafeSitePageHandler>(
          embedder_, triggering_web_contents_, std::move(handler));
}

FeedbackUIConfig::FeedbackUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  chrome::kChromeUIFeedbackHost) {}

bool FeedbackUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return FeedbackUI::IsFeedbackEnabled(
      Profile::FromBrowserContext(browser_context));
}

bool FeedbackUIConfig::ShouldAutoResizeHost() {
  return true;
}

WEB_UI_CONTROLLER_TYPE_IMPL(FeedbackUI)
