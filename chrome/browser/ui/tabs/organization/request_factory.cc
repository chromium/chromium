// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/request_factory.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organization/logging_util.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "content/public/browser/web_contents.h"

namespace {

bool CanUseOptimizationGuide(Profile* profile) {
  return base::FeatureList::IsEnabled(
             optimization_guide::features::kOptimizationGuideModelExecution) &&
         OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
}

void OnLogResults(Profile* profile,
                  std::unique_ptr<optimization_guide::ModelQualityLogEntry>
                      model_quality_log_entry,
                  const TabOrganizationSession* session) {
  if (!model_quality_log_entry) {
    return;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service) {
    return;
  }

  if (!session->request() || !session->request()->response() ||
      session->request()->response()->organizations.size() == 0 ||
      session->tab_organizations().size() == 0) {
    return;
  }

  optimization_guide::proto::TabOrganizationQuality* quality =
      model_quality_log_entry
          ->quality_data<optimization_guide::TabOrganizationFeatureTypeMap>();

  AddSessionDetailsToQuality(quality, session);

  optimization_guide_keyed_service->UploadModelQualityLogs(
      std::move(model_quality_log_entry));
}

void OnTabOrganizationModelExecutionResult(
    Profile* profile,
    TabOrganizationRequest::BackendCompletionCallback on_completion,
    TabOrganizationRequest::BackendFailureCallback on_failure,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.has_value()) {
    LOG(ERROR) << "TabOrganizationResponse model execution failed ";
    std::move(on_failure).Run();
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::TabOrganizationResponse>(result.value());
  if (!response) {
    std::move(on_failure).Run();
    return;
  }

  std::vector<TabOrganizationResponse::Organization> organizations;
  for (const auto& tab_organization : response->tab_organizations()) {
    std::vector<TabData::TabID> response_tab_ids;
    for (const auto& tab : tab_organization.tabs()) {
      response_tab_ids.emplace_back(tab.tab_id());
    }
    organizations.emplace_back(base::UTF8ToUTF16(tab_organization.label()),
                               std::move(response_tab_ids));
  }

  const std::string server_execution_id = log_entry->log_ai_data_request()
                                              ->mutable_model_execution_info()
                                              ->server_execution_id();

  std::unique_ptr<TabOrganizationResponse> local_response =
      std::make_unique<TabOrganizationResponse>(
          std::move(organizations), base::UTF8ToUTF16(server_execution_id),
          base::BindOnce(OnLogResults, profile, std::move(log_entry)));

  std::move(on_completion).Run(std::move(local_response));
}

void PerformTabOrganizationExecution(
    Profile* profile,
    const TabOrganizationRequest* request,
    TabOrganizationRequest::BackendCompletionCallback on_completion,
    TabOrganizationRequest::BackendFailureCallback on_failure) {
  if (!CanUseOptimizationGuide(profile)) {
    std::move(on_failure).Run();
    return;
  }

  optimization_guide::proto::TabOrganizationRequest tab_organization_request;
  for (const std::unique_ptr<TabData>& tab_data : request->tab_datas()) {
    if (!tab_data->IsValidForOrganizing()) {
      continue;
    }

    auto* tab = tab_organization_request.add_tabs();
    tab->set_tab_id(tab_data->tab_id());
    tab->set_title(base::UTF16ToUTF8(tab_data->web_contents()->GetTitle()));
    tab->set_url(tab_data->original_url().spec());
  }

  if (request->base_tab_id().has_value()) {
    tab_organization_request.set_active_tab_id(request->base_tab_id().value());
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  optimization_guide_keyed_service->ExecuteModel(
      optimization_guide::proto::ModelExecutionFeature::
          MODEL_EXECUTION_FEATURE_TAB_ORGANIZATION,
      tab_organization_request,
      base::BindOnce(OnTabOrganizationModelExecutionResult, profile,
                     std::move(on_completion), std::move(on_failure)));
}

}  // anonymous namespace

TabOrganizationRequestFactory::~TabOrganizationRequestFactory() = default;

// static
std::unique_ptr<TabOrganizationRequestFactory>
TabOrganizationRequestFactory::GetForProfile(Profile* profile) {
  if (CanUseOptimizationGuide(profile)) {
    return std::make_unique<OptimizationGuideTabOrganizationRequestFactory>();
  }
  return std::make_unique<TwoTabsRequestFactory>();
}

TwoTabsRequestFactory::~TwoTabsRequestFactory() = default;

std::unique_ptr<TabOrganizationRequest> TwoTabsRequestFactory::CreateRequest(
    Profile* profile) {
  // for this request strategy only the first 2 tabs will be addedto an
  // organization.
  TabOrganizationRequest::BackendStartRequest start_request = base::BindOnce(
      [](const TabOrganizationRequest* request,
         TabOrganizationRequest::BackendCompletionCallback on_completion,
         TabOrganizationRequest::BackendFailureCallback on_failure) {
        if (request->tab_datas().size() >= 2) {
          std::vector<TabData::TabID> response_tab_ids;
          std::transform(request->tab_datas().begin(),
                         request->tab_datas().begin() + 2,
                         std::back_inserter(response_tab_ids),
                         [](const std::unique_ptr<TabData>& tab_data) {
                           return tab_data->tab_id();
                         });

          std::vector<TabOrganizationResponse::Organization> organizations;
          organizations.emplace_back(u"Organization",
                                     std::move(response_tab_ids));

          std::unique_ptr<TabOrganizationResponse> response =
              std::make_unique<TabOrganizationResponse>(
                  std::move(organizations));

          std::move(on_completion).Run(std::move(response));
        } else {
          std::move(on_failure).Run();
        }
      });

  return std::make_unique<TabOrganizationRequest>(std::move(start_request));
}

OptimizationGuideTabOrganizationRequestFactory::
    ~OptimizationGuideTabOrganizationRequestFactory() = default;

std::unique_ptr<TabOrganizationRequest>
OptimizationGuideTabOrganizationRequestFactory::CreateRequest(
    Profile* profile) {
  return std::make_unique<TabOrganizationRequest>(
      base::BindOnce(PerformTabOrganizationExecution, profile));
}
