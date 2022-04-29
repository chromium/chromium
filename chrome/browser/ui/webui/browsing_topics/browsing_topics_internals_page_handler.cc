// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_page_handler.h"

#include <utility>

#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"

BrowsingTopicsInternalsPageHandler::BrowsingTopicsInternalsPageHandler(
    Profile* profile,
    mojo::PendingReceiver<browsing_topics::mojom::PageHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {}

BrowsingTopicsInternalsPageHandler::~BrowsingTopicsInternalsPageHandler() =
    default;

void BrowsingTopicsInternalsPageHandler::GetBrowsingTopicsConfiguration(
    browsing_topics::mojom::PageHandler::GetBrowsingTopicsConfigurationCallback
        callback) {
  auto config = browsing_topics::mojom::WebUIBrowsingTopicsConfiguration::New(
      base::FeatureList::IsEnabled(blink::features::kBrowsingTopics),
      base::FeatureList::IsEnabled(features::kPrivacySandboxAdsAPIsOverride),
      base::FeatureList::IsEnabled(privacy_sandbox::kPrivacySandboxSettings3),
      base::FeatureList::IsEnabled(
          privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting),
      base::FeatureList::IsEnabled(
          blink::features::kBrowsingTopicsBypassIPIsPubliclyRoutableCheck),
      blink::features::kBrowsingTopicsNumberOfEpochsToExpose.Get(),
      blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get(),
      blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get(),
      blink::features::kBrowsingTopicsUseRandomTopicProbabilityPercent.Get(),
      blink::features::
          kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering.Get(),
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic.Get(),
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch.Get(),
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad
              .Get(),
      blink::features::kBrowsingTopicsConfigVersion.Get(),
      blink::features::kBrowsingTopicsTaxonomyVersion.Get());

  std::move(callback).Run(std::move(config));
}

void BrowsingTopicsInternalsPageHandler::GetBrowsingTopicsState(
    browsing_topics::mojom::PageHandler::GetBrowsingTopicsStateCallback
        callback) {
  browsing_topics::BrowsingTopicsService* browsing_topics_service =
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile_);

  if (!browsing_topics_service) {
    std::move(callback).Run(
        browsing_topics::mojom::WebUIGetBrowsingTopicsStateResult::
            NewOverrideStatusMessage("No BrowsingTopicsService: the "
                                     "\"BrowsingTopics\" or other depend-on "
                                     "features are disabled."));
    return;
  }

  std::move(callback).Run(
      browsing_topics_service->GetBrowsingTopicsStateForWebUi());
}
