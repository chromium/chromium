// Copyright 2022 The Chromium Authors
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
      base::FeatureList::IsEnabled(
          privacy_sandbox::kOverridePrivacySandboxSettingsLocalTesting),
      base::FeatureList::IsEnabled(
          blink::features::kBrowsingTopicsBypassIPIsPubliclyRoutableCheck),
      base::FeatureList::IsEnabled(blink::features::kBrowsingTopicsDocumentAPI),
      browsing_topics::CurrentConfigVersion(),
      base::FeatureList::IsEnabled(blink::features::kBrowsingTopicsParameters),
      blink::features::kBrowsingTopicsNumberOfEpochsToExpose.Get(),
      blink::features::kBrowsingTopicsTimePeriodPerEpoch.Get(),
      blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get(),
      blink::features::kBrowsingTopicsUseRandomTopicProbabilityPercent.Get(),
      blink::features::kBrowsingTopicsMaxEpochIntroductionDelay.Get(),
      blink::features::
          kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering.Get(),
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic.Get(),
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch.Get(),
      blink::features::
          kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad
              .Get(),
      blink::features::kBrowsingTopicsTaxonomyVersion.Get(),
      blink::features::kBrowsingTopicsDisabledTopicsList.Get());

  std::move(callback).Run(std::move(config));
}

void BrowsingTopicsInternalsPageHandler::GetBrowsingTopicsState(
    bool calculate_now,
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

  browsing_topics_service->GetBrowsingTopicsStateForWebUi(calculate_now,
                                                          std::move(callback));
}

void BrowsingTopicsInternalsPageHandler::GetModelInfo(
    browsing_topics::mojom::PageHandler::GetModelInfoCallback callback) {
  browsing_topics::BrowsingTopicsService* browsing_topics_service =
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile_);

  if (!browsing_topics_service) {
    std::move(callback).Run(
        browsing_topics::mojom::WebUIGetModelInfoResult::
            NewOverrideStatusMessage("No BrowsingTopicsService: the "
                                     "\"BrowsingTopics\" or other depend-on "
                                     "features are disabled."));
    return;
  }

  auto* annotator = browsing_topics_service->GetAnnotator();
  if (!annotator) {
    std::move(callback).Run(browsing_topics::mojom::WebUIGetModelInfoResult::
                                NewOverrideStatusMessage(
                                    "BrowsingTopicsService is shutting down."));
    return;
  }

  annotator->NotifyWhenModelAvailable(base::BindOnce(
      &BrowsingTopicsInternalsPageHandler::OnGetModelInfoCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowsingTopicsInternalsPageHandler::ClassifyHosts(
    const std::vector<std::string>& hosts,
    browsing_topics::mojom::PageHandler::ClassifyHostsCallback callback) {
  if (hosts.empty()) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to call ClassifyHosts() with empty `hosts`.");
    return;
  }

  browsing_topics::BrowsingTopicsService* browsing_topics_service =
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile_);

  if (!browsing_topics_service) {
    std::move(callback).Run({});
    return;
  }
  auto* annotator = browsing_topics_service->GetAnnotator();
  if (!annotator) {
    std::move(callback).Run({});
    return;
  }

  annotator->BatchAnnotate(
      base::BindOnce(
          &BrowsingTopicsInternalsPageHandler::OnGetTopicsForHostsCompleted,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      hosts);
}

void BrowsingTopicsInternalsPageHandler::OnGetModelInfoCompleted(
    browsing_topics::mojom::PageHandler::GetModelInfoCallback callback) {
  browsing_topics::BrowsingTopicsService* browsing_topics_service =
      browsing_topics::BrowsingTopicsServiceFactory::GetForProfile(profile_);
  DCHECK(browsing_topics_service);

  auto* annotator = browsing_topics_service->GetAnnotator();
  if (!annotator) {
    std::move(callback).Run(browsing_topics::mojom::WebUIGetModelInfoResult::
                                NewOverrideStatusMessage(
                                    "BrowsingTopicsService is shutting down."));
    return;
  }

  std::optional<optimization_guide::ModelInfo> model_info =
      annotator->GetBrowsingTopicsModelInfo();

  if (!model_info) {
    std::move(callback).Run(browsing_topics::mojom::WebUIGetModelInfoResult::
                                NewOverrideStatusMessage("Model unavailable."));
    return;
  }

  auto webui_model_info = browsing_topics::mojom::WebUIModelInfo::New();
  webui_model_info->model_version =
      base::NumberToString(model_info->GetVersion());
  webui_model_info->model_file_path =
      model_info->GetModelFilePath().AsUTF8Unsafe();

  std::move(callback).Run(
      browsing_topics::mojom::WebUIGetModelInfoResult::NewModelInfo(
          std::move(webui_model_info)));
}

void BrowsingTopicsInternalsPageHandler::OnGetTopicsForHostsCompleted(
    browsing_topics::mojom::PageHandler::ClassifyHostsCallback callback,
    const std::vector<browsing_topics::Annotation>& annotations) {
  std::vector<std::vector<browsing_topics::mojom::WebUITopicPtr>>
      webui_topics_for_hosts;

  for (const browsing_topics::Annotation& annotation : annotations) {
    std::vector<browsing_topics::mojom::WebUITopicPtr> webui_topics_for_host;

    for (int32_t topic_id : annotation.topics) {
      browsing_topics::Topic topic = browsing_topics::Topic(topic_id);
      privacy_sandbox::CanonicalTopic canonical_topic =
          privacy_sandbox::CanonicalTopic(
              topic, blink::features::kBrowsingTopicsTaxonomyVersion.Get());

      browsing_topics::mojom::WebUITopicPtr webui_topic =
          browsing_topics::mojom::WebUITopic::New();
      webui_topic->topic_id = topic.value();
      webui_topic->topic_name = canonical_topic.GetLocalizedRepresentation();
      webui_topic->is_real_topic = true;

      webui_topics_for_host.push_back(std::move(webui_topic));
    }

    webui_topics_for_hosts.push_back(std::move(webui_topics_for_host));
  }

  std::move(callback).Run(std::move(webui_topics_for_hosts));
}
