// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/about_this_site_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace page_info {
namespace {
using AboutThisSiteStatus = about_this_site_validation::AboutThisSiteStatus;
using AboutThisSiteInteraction = AboutThisSiteService::AboutThisSiteInteraction;
using OptimizationGuideDecision = optimization_guide::OptimizationGuideDecision;

void RecordAboutThisSiteInteraction(AboutThisSiteInteraction interaction) {
  base::UmaHistogramEnumeration("Security.PageInfo.AboutThisSiteInteraction",
                                interaction);
}

}  // namespace

AboutThisSiteService::AboutThisSiteService(
    std::unique_ptr<Client> client,
    TemplateURLService* template_url_service,
    bool allow_missing_description)
    : client_(std::move(client)),
      template_url_service_(template_url_service),
      allow_missing_description_(allow_missing_description) {}

absl::optional<proto::SiteInfo> AboutThisSiteService::GetAboutThisSiteInfo(
    const GURL& url,
    ukm::SourceId source_id) const {
  if (!search::DefaultSearchProviderIsGoogle(template_url_service_)) {
    RecordAboutThisSiteInteraction(
        AboutThisSiteInteraction::kNotShownNonGoogleDSE);

    return absl::nullopt;
  }

  optimization_guide::OptimizationMetadata metadata;
  auto decision = client_->CanApplyOptimization(url, &metadata);
  absl::optional<proto::AboutThisSiteMetadata> about_this_site_metadata =
      metadata.ParsedMetadata<proto::AboutThisSiteMetadata>();

  AboutThisSiteStatus status =
      decision == OptimizationGuideDecision::kUnknown
          ? AboutThisSiteStatus::kUnknown
          : about_this_site_validation::ValidateMetadata(
                about_this_site_metadata, allow_missing_description_);
  base::UmaHistogramEnumeration("Security.PageInfo.AboutThisSiteStatus",
                                status);
  RecordAboutThisSiteInteraction(
      status == AboutThisSiteStatus::kValid
          ? (about_this_site_metadata->site_info().has_description()
                 ? AboutThisSiteInteraction::kShownWithDescription
                 : AboutThisSiteInteraction::kShownWithoutDescription)
          : AboutThisSiteInteraction::kNotShown);

  ukm::builders::AboutThisSiteStatus(source_id)
      .SetStatus(static_cast<int>(status))
      .Record(ukm::UkmRecorder::Get());
  if (status == AboutThisSiteStatus::kValid) {
    if (about_this_site_metadata->site_info().has_more_about()) {
      // Append a context parameter to identify that this URL is visited from
      // Chrome. If we add more UI surfaces that can open this URL, we should
      // pass in different context parameters.
      proto::MoreAbout* more_about =
          about_this_site_metadata->mutable_site_info()->mutable_more_about();
      GURL more_about_url =
          net::AppendQueryParameter(GURL(more_about->url()), "ctx", "chrome");
      more_about->set_url(more_about_url.spec());
    }
    return about_this_site_metadata->site_info();
  }

  if (kShowSampleContent.Get()) {
    page_info::proto::SiteInfo site_info;
    if (url == GURL("https://example.com")) {
      if (!allow_missing_description_) {
        auto* description = site_info.mutable_description();
        description->set_name("Example website");
        description->set_subtitle("Website");
        description->set_description(
            "A domain used in illustrative examples in documents.");
        description->mutable_source()->set_url("https://example.com");
        description->mutable_source()->set_label("Example source");
      }
      site_info.mutable_more_about()->set_url(
          "https://example.com/#more-about");
      return site_info;
    }

    if (url == GURL("https://permission.site")) {
      auto* description = site_info.mutable_description();
      description->set_name("Permission Site");
      description->set_subtitle("Testing site");
      description->set_description(
          "A site containing test buttons for various browser APIs, in order"
          " to trigger permission dialogues and similar UI in modern "
          "browsers.");
      description->mutable_source()->set_url("https://permission.site.com");
      description->mutable_source()->set_label("Permission Site");
      return site_info;
    }
  }

  return absl::nullopt;
}

// static
void AboutThisSiteService::OnAboutThisSiteRowClicked(bool with_description) {
  RecordAboutThisSiteInteraction(
      with_description ? AboutThisSiteInteraction::kClickedWithDescription
                       : AboutThisSiteInteraction::kClickedWithoutDescription);
}

// static
void AboutThisSiteService::OnOpenedDirectlyFromSidePanel() {
  RecordAboutThisSiteInteraction(
      AboutThisSiteInteraction::kOpenedDirectlyFromSidePanel);
}

base::WeakPtr<AboutThisSiteService> AboutThisSiteService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AboutThisSiteService::~AboutThisSiteService() = default;

}  // namespace page_info
