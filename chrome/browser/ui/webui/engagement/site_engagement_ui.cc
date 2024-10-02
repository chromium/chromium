// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/engagement_resources.h"
#include "chrome/grit/engagement_resources_map.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

// Implementation of site_engagement::mojom::SiteEngagementDetailsProvider that
// gets information from the site_engagement::SiteEngagementService to provide
// data for the WebUI.
class SiteEngagementDetailsProviderImpl
    : public site_engagement::mojom::SiteEngagementDetailsProvider {
 public:
  // Instance is deleted when the supplied pipe is destroyed.
  SiteEngagementDetailsProviderImpl(
      Profile* profile,
      mojo::PendingReceiver<
          site_engagement::mojom::SiteEngagementDetailsProvider> receiver)
      : profile_(profile), receiver_(this, std::move(receiver)) {
    DCHECK(profile_);
  }

  SiteEngagementDetailsProviderImpl(const SiteEngagementDetailsProviderImpl&) =
      delete;
  SiteEngagementDetailsProviderImpl& operator=(
      const SiteEngagementDetailsProviderImpl&) = delete;

  ~SiteEngagementDetailsProviderImpl() override {}

  // site_engagement::mojom::SiteEngagementDetailsProvider overrides:
  void GetSiteEngagementDetails(
      GetSiteEngagementDetailsCallback callback) override {
    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(profile_);

    std::vector<site_engagement::mojom::SiteEngagementDetails> scores =
        service->GetAllDetails(
            site_engagement::SiteEngagementService::URLSets::HTTP |
            site_engagement::SiteEngagementService::URLSets::WEB_UI);

    std::vector<site_engagement::mojom::SiteEngagementDetailsPtr>
        engagement_info;
    engagement_info.reserve(scores.size());
    for (const auto& info : scores) {
      site_engagement::mojom::SiteEngagementDetailsPtr origin_info(
          site_engagement::mojom::SiteEngagementDetails::New());
      *origin_info = std::move(info);
      engagement_info.push_back(std::move(origin_info));
    }

    std::move(callback).Run(std::move(engagement_info));
  }

  void SetSiteEngagementBaseScoreForUrl(const GURL& origin,
                                        double score) override {
    if (!origin.is_valid() || !origin.SchemeIsHTTPOrHTTPS() || score < 0 ||
        score > site_engagement::SiteEngagementService::GetMaxPoints() ||
        std::isnan(score)) {
      return;
    }

    site_engagement::SiteEngagementService* service =
        site_engagement::SiteEngagementService::Get(profile_);
    service->ResetBaseScoreForURL(origin, score);
  }

 private:
  // The Profile* handed to us in our constructor.
  raw_ptr<Profile> profile_;

  mojo::Receiver<site_engagement::mojom::SiteEngagementDetailsProvider>
      receiver_;
};

}  // namespace

bool SiteEngagementUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return site_engagement::SiteEngagementService::IsEnabled();
}

SiteEngagementUI::SiteEngagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://site-engagement/ source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUISiteEngagementHost);
  webui::SetupWebUIDataSource(
      source, base::make_span(kEngagementResources, kEngagementResourcesSize),
      IDR_ENGAGEMENT_SITE_ENGAGEMENT_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(SiteEngagementUI)

SiteEngagementUI::~SiteEngagementUI() {}

void SiteEngagementUI::BindInterface(
    mojo::PendingReceiver<site_engagement::mojom::SiteEngagementDetailsProvider>
        receiver) {
  ui_handler_ = std::make_unique<SiteEngagementDetailsProviderImpl>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
