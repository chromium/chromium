// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/request_factory.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_request.h"

TabOrganizationRequestFactory::~TabOrganizationRequestFactory() = default;

TwoTabsRequestFactory::~TwoTabsRequestFactory() = default;

std::unique_ptr<TabOrganizationRequest> TwoTabsRequestFactory::CreateRequest() {
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
