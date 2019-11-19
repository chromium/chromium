// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace {

// Implementation of mojom::SiteEngagementDetailsProvider that gets information
// from the SiteEngagementService to provide data for the WebUI.
class SiteEngagementDetailsProviderImpl
    : public mojom::SiteEngagementDetailsProvider {
 public:
  // Instance is deleted when the supplied pipe is destroyed.
  SiteEngagementDetailsProviderImpl(
      Profile* profile,
      mojo::PendingReceiver<mojom::SiteEngagementDetailsProvider> receiver)
      : profile_(profile), receiver_(this, std::move(receiver)) {
    DCHECK(profile_);
  }

  ~SiteEngagementDetailsProviderImpl() override {}

  // mojom::SiteEngagementDetailsProvider overrides:
  void GetSiteEngagementDetails(
      GetSiteEngagementDetailsCallback callback) override {
    SiteEngagementService* service = SiteEngagementService::Get(profile_);
    std::vector<mojom::SiteEngagementDetails> scores = service->GetAllDetails();

    std::vector<mojom::SiteEngagementDetailsPtr> engagement_info;
    engagement_info.reserve(scores.size());
    for (const auto& info : scores) {
      mojom::SiteEngagementDetailsPtr origin_info(
          mojom::SiteEngagementDetails::New());
      *origin_info = std::move(info);
      engagement_info.push_back(std::move(origin_info));
    }

    std::move(callback).Run(std::move(engagement_info));
  }

  void SetSiteEngagementBaseScoreForUrl(const GURL& origin,
                                        double score) override {
    if (!origin.is_valid() || score < 0 ||
        score > SiteEngagementService::GetMaxPoints() || std::isnan(score)) {
      return;
    }

    SiteEngagementService* service = SiteEngagementService::Get(profile_);
    service->ResetBaseScoreForURL(origin, score);
  }

 private:
  // The Profile* handed to us in our constructor.
  Profile* profile_;

  mojo::Receiver<mojom::SiteEngagementDetailsProvider> receiver_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementDetailsProviderImpl);
};

}  // namespace

SiteEngagementUI::SiteEngagementUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://site-engagement/ source.
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUISiteEngagementHost));
  source->AddResourcePath("site_engagement.js", IDR_SITE_ENGAGEMENT_JS);
  source->AddResourcePath(
      "chrome/browser/engagement/site_engagement_details.mojom-lite.js",
      IDR_SITE_ENGAGEMENT_DETAILS_MOJOM_LITE_JS);
  source->SetDefaultResource(IDR_SITE_ENGAGEMENT_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source.release());

  AddHandlerToRegistry(
      base::BindRepeating(&SiteEngagementUI::BindSiteEngagementDetailsProvider,
                          base::Unretained(this)));
}

SiteEngagementUI::~SiteEngagementUI() {}

void SiteEngagementUI::BindSiteEngagementDetailsProvider(
    mojo::PendingReceiver<mojom::SiteEngagementDetailsProvider> receiver) {
  ui_handler_ = std::make_unique<SiteEngagementDetailsProviderImpl>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
