// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/cast_feedback_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/media_router_feedback_resources.h"
#include "chrome/grit/media_router_feedback_resources_map.h"
#include "components/media_router/browser/logger_impl.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

CastFeedbackUI::CastFeedbackUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUICastFeedbackHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"additionalComments", IDS_MEDIA_ROUTER_FEEDBACK_ADDITIONAL_COMMENTS},
      {"allowContactByEmail", IDS_MEDIA_ROUTER_FEEDBACK_ALLOW_CONTACT_BY_EMAIL},
      {"audioAcceptable", IDS_MEDIA_ROUTER_FEEDBACK_AUDIO_ACCEPTABLE},
      {"audioGood", IDS_MEDIA_ROUTER_FEEDBACK_AUDIO_GOOD},
      {"audioPerfect", IDS_MEDIA_ROUTER_FEEDBACK_AUDIO_PERFECT},
      {"audioPoor", IDS_MEDIA_ROUTER_FEEDBACK_AUDIO_POOR},
      {"audioQuality", IDS_MEDIA_ROUTER_FEEDBACK_AUDIO_QUALITY},
      {"audioUnintelligible", IDS_MEDIA_ROUTER_FEEDBACK_AUDIO_UNINTELLIGIBLE},
      {"cancel", IDS_CANCEL},
      {"contentQuestion", IDS_MEDIA_ROUTER_FEEDBACK_CONTENT_QUESTION},
      {"didNotTry", IDS_MEDIA_ROUTER_FEEDBACK_DID_NOT_TRY},
      {"discardConfirmation", IDS_MEDIA_ROUTER_FEEDBACK_DISCARD_CONFIRMATION},
      {"emailField", IDS_MEDIA_ROUTER_FEEDBACK_EMAIL_FIELD},
      {"fineLogsWarning", IDS_MEDIA_ROUTER_FEEDBACK_FINE_LOGS_WARNING},
      {"header", IDS_MEDIA_ROUTER_FEEDBACK_HEADER},
      {"logsHeader", IDS_MEDIA_ROUTER_FEEDBACK_LOGS_HEADER},
      {"mirroringQualitySubheading",
       IDS_MEDIA_ROUTER_FEEDBACK_MIRRORING_QUALITY_SUBHEADING},
      {"na", IDS_MEDIA_ROUTER_FEEDBACK_NA},
      {"networkDifferentWifi",
       IDS_MEDIA_ROUTER_FEEDBACK_NETWORK_DIFFERENT_WIFI},
      {"networkQuestion", IDS_MEDIA_ROUTER_FEEDBACK_NETWORK_QUESTION},
      {"networkSameWifi", IDS_MEDIA_ROUTER_FEEDBACK_NETWORK_SAME_WIFI},
      {"networkWiredPc", IDS_MEDIA_ROUTER_FEEDBACK_NETWORK_WIRED_PC},
      {"no", IDS_MEDIA_ROUTER_FEEDBACK_NO},
      {"ok", IDS_OK},
      {"privacyDataUsage", IDS_MEDIA_ROUTER_FEEDBACK_PRIVACY_DATA_USAGE},
      {"prompt", IDS_MEDIA_ROUTER_FEEDBACK_PROMPT},
      {"required", IDS_MEDIA_ROUTER_FEEDBACK_REQUIRED},
      {"resending", IDS_MEDIA_ROUTER_FEEDBACK_RESENDING},
      {"sendButton", IDS_MEDIA_ROUTER_FEEDBACK_SEND_BUTTON},
      {"sendFail", IDS_MEDIA_ROUTER_FEEDBACK_SEND_FAIL},
      {"sendLogs", IDS_MEDIA_ROUTER_FEEDBACK_SEND_LOGS},
      {"sendLogsHtml", IDS_MEDIA_ROUTER_FEEDBACK_SEND_LOGS_HTML},
      {"sendSuccess", IDS_MEDIA_ROUTER_FEEDBACK_SEND_SUCCESS},
      {"sending", IDS_MEDIA_ROUTER_FEEDBACK_SENDING},
      {"softwareQuestion", IDS_MEDIA_ROUTER_FEEDBACK_SOFTWARE_QUESTION},
      {"title", IDS_MEDIA_ROUTER_FEEDBACK_TITLE},
      {"typeBugOrError", IDS_MEDIA_ROUTER_FEEDBACK_TYPE_BUG_OR_ERROR},
      {"typeDiscovery", IDS_MEDIA_ROUTER_FEEDBACK_TYPE_DISCOVERY},
      {"typeFeatureRequest", IDS_MEDIA_ROUTER_FEEDBACK_TYPE_FEATURE_REQUEST},
      {"typeOther", IDS_MEDIA_ROUTER_FEEDBACK_TYPE_OTHER},
      {"typeProjectionQuality",
       IDS_MEDIA_ROUTER_FEEDBACK_TYPE_PROJECTION_QUALITY},
      {"typeQuestion", IDS_MEDIA_ROUTER_FEEDBACK_TYPE_QUESTION},
      {"unknown", IDS_MEDIA_ROUTER_FEEDBACK_UNKNOWN},
      {"videoAcceptable", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_ACCEPTABLE},
      {"videoFreezes", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_FREEZES},
      {"videoGood", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_GOOD},
      {"videoGreat", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_GREAT},
      {"videoJerky", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_JERKY},
      {"videoPerfect", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_PERFECT},
      {"videoPoor", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_POOR},
      {"videoQuality", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_QUALITY},
      {"videoSmooth", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_SMOOTH},
      {"videoSmoothness", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_SMOOTHNESS},
      {"videoStutter", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_STUTTER},
      {"videoUnwatchable", IDS_MEDIA_ROUTER_FEEDBACK_VIDEO_UNWATCHABLE},
      {"yes", IDS_MEDIA_ROUTER_FEEDBACK_YES},
      {"yourAnswer", IDS_MEDIA_ROUTER_FEEDBACK_YOUR_ANSWER},
      {"yourEmailAddress", IDS_MEDIA_ROUTER_FEEDBACK_YOUR_EMAIL_ADDRESS},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddString(
      "formDescription",
      l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_FEEDBACK_FORM_DESCRIPTION,
          u"https://support.google.com/chromecast?p=troubleshoot_chromecast"));
  source->AddString(
      "setupVisibilityQuestion",
      l10n_util::GetStringFUTF8(
          IDS_MEDIA_ROUTER_FEEDBACK_SETUP_VISIBILITY_QUESTION,
          u"https://support.google.com/chromecast?p=set_up_chromecast"));

  // Supply media router log data to UI.
  MediaRouter* const router =
      media_router::MediaRouterFactory::GetApiForBrowserContext(
          web_contents_->GetBrowserContext());

  std::string log_data;

  // Reserve a few kb of space to hopefully avoid multiple allocations later.
  log_data.reserve(8192);

  JSONStringValueSerializer serializer(&log_data);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(router->GetState())) {
    log_data.clear();
  }

  LoggerImpl* const logger = router->GetLogger();
  if (logger) {
    log_data += logger->GetLogsAsJson();
  }

  MediaRouterDebugger& debugger = router->GetDebugger();
  if (debugger.ShouldFetchMirroringStats()) {
    std::string mirroring_stats_json;
    JSONStringValueSerializer mirroring_stats_serializer(&mirroring_stats_json);
    mirroring_stats_serializer.set_pretty_print(true);
    if (mirroring_stats_serializer.Serialize(debugger.GetMirroringStats())) {
      log_data += mirroring_stats_json;
    }
  }

  // If there is any log data, add it to the `source`.
  if (!log_data.empty()) {
    source->AddString("logData", log_data);
  }

  // As the name suggests, this value is used to categorize feedback reports for
  // easier analysis and triage.
  source->AddString(
      "categoryTag",
      std::string(version_info::GetChannelString(chrome::GetChannel())));

  source->AddBoolean("globalMediaControlsCastStartStop",
                     GlobalMediaControlsCastStartStopEnabled(profile_));

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kMediaRouterFeedbackResources,
                      kMediaRouterFeedbackResourcesSize),
      IDR_MEDIA_ROUTER_FEEDBACK_FEEDBACK_HTML);

  web_ui->RegisterMessageCallback(
      "close", base::BindRepeating(&CastFeedbackUI::OnCloseMessage,
                                   base::Unretained(this)));
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());
}

WEB_UI_CONTROLLER_TYPE_IMPL(CastFeedbackUI)

CastFeedbackUI::~CastFeedbackUI() = default;

void CastFeedbackUI::OnCloseMessage(const base::Value::List&) {
  web_contents_->GetDelegate()->CloseContents(web_contents_);
}

CastFeedbackUIConfig::CastFeedbackUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUICastFeedbackHost) {}

bool CastFeedbackUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return MediaRouterEnabled(profile) && FeedbackUI::IsFeedbackEnabled(profile);
}

}  // namespace media_router
