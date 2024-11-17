// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_engagement_ui.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/media_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

namespace {

namespace {

// This is the component ID for the MEI Preload component.
const char kPreloadComponentID[] = "aemomkdncapdnfajjbbcbdebjljbpmpj";

}  // namespace

// Implementation of media::mojom::MediaEngagementScoreDetailsProvider that
// retrieves engagement details from the MediaEngagementService.
class MediaEngagementScoreDetailsProviderImpl
    : public media::mojom::MediaEngagementScoreDetailsProvider {
 public:
  MediaEngagementScoreDetailsProviderImpl(
      content::WebUI* web_ui,
      mojo::PendingReceiver<media::mojom::MediaEngagementScoreDetailsProvider>
          receiver)
      : web_ui_(web_ui),
        profile_(Profile::FromWebUI(web_ui)),
        receiver_(this, std::move(receiver)) {
    DCHECK(web_ui_);
    DCHECK(profile_);
    service_ = MediaEngagementService::Get(profile_);
  }

  MediaEngagementScoreDetailsProviderImpl(
      const MediaEngagementScoreDetailsProviderImpl&) = delete;
  MediaEngagementScoreDetailsProviderImpl& operator=(
      const MediaEngagementScoreDetailsProviderImpl&) = delete;

  ~MediaEngagementScoreDetailsProviderImpl() override {}

  // media::mojom::MediaEngagementScoreDetailsProvider overrides:
  void GetMediaEngagementScoreDetails(
      media::mojom::MediaEngagementScoreDetailsProvider::
          GetMediaEngagementScoreDetailsCallback callback) override {
    std::move(callback).Run(service_->GetAllScoreDetails());
  }

  void GetMediaEngagementConfig(
      media::mojom::MediaEngagementScoreDetailsProvider::
          GetMediaEngagementConfigCallback callback) override {
    std::move(callback).Run(media::mojom::MediaEngagementConfig::New(
        MediaEngagementScore::GetScoreMinVisits(),
        MediaEngagementScore::GetHighScoreLowerThreshold(),
        MediaEngagementScore::GetHighScoreUpperThreshold(),
        base::FeatureList::IsEnabled(media::kRecordMediaEngagementScores),
        base::FeatureList::IsEnabled(
            media::kMediaEngagementBypassAutoplayPolicies),
        base::FeatureList::IsEnabled(media::kPreloadMediaEngagementData),
        base::FeatureList::IsEnabled(media::kMediaEngagementHTTPSOnly),
        base::FeatureList::IsEnabled(media::kAutoplayDisableSettings),
        GetBlockAutoplayPref(),
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAutoplayPolicy),
        GetAppliedAutoplayPolicy(), GetPreloadVersion()));
  }

 private:
  const std::string GetAppliedAutoplayPolicy() {
    switch (web_ui_->GetWebContents()
                ->GetOrCreateWebPreferences()
                .autoplay_policy) {
      case blink::mojom::AutoplayPolicy::kNoUserGestureRequired:
        return "no-user-gesture-required";
      case blink::mojom::AutoplayPolicy::kUserGestureRequired:
        return "user-gesture-required";
      case blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired:
        return "document-user-activation-required";
    }
  }

  const std::string GetPreloadVersion() {
    component_updater::ComponentUpdateService* cus =
        g_browser_process->component_updater();
    std::vector<component_updater::ComponentInfo> info = cus->GetComponents();

    for (const auto& component : info) {
      if (component.id == kPreloadComponentID)
        return component.version.GetString();
    }

    return std::string();
  }

  // Pref is not available on Android.
  bool GetBlockAutoplayPref() {
#if BUILDFLAG(IS_ANDROID)
    return false;
#else
    return profile_->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
#endif
  }

  raw_ptr<content::WebUI> web_ui_;

  raw_ptr<Profile> profile_;

  raw_ptr<MediaEngagementService> service_;

  mojo::Receiver<media::mojom::MediaEngagementScoreDetailsProvider> receiver_;
};

}  // namespace

bool MediaEngagementUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return MediaEngagementService::IsEnabled();
}

MediaEngagementUI::MediaEngagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Setup the data source behind chrome://media-engagement.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIMediaEngagementHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  source->AddResourcePath("media_engagement.js", IDR_MEDIA_MEDIA_ENGAGEMENT_JS);
  source->AddResourcePath(
      "media_engagement_score_details.mojom-webui.js",
      IDR_MEDIA_MEDIA_ENGAGEMENT_SCORE_DETAILS_MOJOM_WEBUI_JS);
  source->SetDefaultResource(IDR_MEDIA_MEDIA_ENGAGEMENT_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaEngagementUI)

MediaEngagementUI::~MediaEngagementUI() = default;

void MediaEngagementUI::BindInterface(
    mojo::PendingReceiver<media::mojom::MediaEngagementScoreDetailsProvider>
        receiver) {
  ui_handler_ = std::make_unique<MediaEngagementScoreDetailsProviderImpl>(
      web_ui(), std::move(receiver));
}
