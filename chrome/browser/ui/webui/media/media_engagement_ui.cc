// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_engagement_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/media_engagement_score.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/web_preferences.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if !defined(OS_ANDROID)
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
        base::FeatureList::IsEnabled(media::kAutoplayWhitelistSettings),
        GetBlockAutoplayPref(),
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAutoplayPolicy),
        GetAppliedAutoplayPolicy(), GetPreloadVersion()));
  }

 private:
  const std::string GetAppliedAutoplayPolicy() {
    switch (web_ui_->GetWebContents()
                ->GetRenderViewHost()
                ->GetWebkitPreferences()
                .autoplay_policy) {
      case content::AutoplayPolicy::kNoUserGestureRequired:
        return "no-user-gesture-required";
      case content::AutoplayPolicy::kUserGestureRequired:
        return "user-gesture-required";
      case content::AutoplayPolicy::kDocumentUserActivationRequired:
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
#if defined(OS_ANDROID)
    return false;
#else
    return profile_->GetPrefs()->GetBoolean(prefs::kBlockAutoplayEnabled);
#endif
  }

  content::WebUI* web_ui_;

  Profile* profile_;

  MediaEngagementService* service_;

  mojo::Receiver<media::mojom::MediaEngagementScoreDetailsProvider> receiver_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementScoreDetailsProviderImpl);
};

}  // namespace

MediaEngagementUI::MediaEngagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Setup the data source behind chrome://media-engagement.
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIMediaEngagementHost));
  source->AddResourcePath("media-engagement.js", IDR_MEDIA_ENGAGEMENT_JS);
  source->AddResourcePath(
      "chrome/browser/media/media_engagement_score_details.mojom-lite.js",
      IDR_MEDIA_ENGAGEMENT_SCORE_DETAILS_MOJOM_LITE_JS);
  source->SetDefaultResource(IDR_MEDIA_ENGAGEMENT_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());
  AddHandlerToRegistry(base::BindRepeating(
      &MediaEngagementUI::BindMediaEngagementScoreDetailsProvider,
      base::Unretained(this)));
}

MediaEngagementUI::~MediaEngagementUI() = default;

void MediaEngagementUI::BindMediaEngagementScoreDetailsProvider(
    mojo::PendingReceiver<media::mojom::MediaEngagementScoreDetailsProvider>
        receiver) {
  ui_handler_ = std::make_unique<MediaEngagementScoreDetailsProviderImpl>(
      web_ui(), std::move(receiver));
}
