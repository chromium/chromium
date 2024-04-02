// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/organization/tab_data.h"
#include "chrome/browser/ui/tabs/organization/tab_organization.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_session.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"

void AddOrganizationDetailsToQualityOrganization(
    optimization_guide::proto::TabOrganizationQuality* quality,
    const TabOrganization* organization,
    const TabOrganizationResponse::Organization* response_organization) {
  CHECK(quality && response_organization);

  optimization_guide::proto::TabOrganizationQuality_Organization*
      quality_organization = quality->add_organizations();
  CHECK(quality_organization != nullptr);

  if (!organization) {
    quality_organization->set_choice(
        optimization_guide::proto::
            TabOrganizationQuality_Organization_Choice_NOT_USED);
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kMultiTabOrganization)) {
    quality_organization->set_user_feedback(organization->feedback());
  }

  switch (organization->choice()) {
    case TabOrganization::UserChoice::kRejected: {
      quality_organization->set_choice(
          optimization_guide::proto::
              TabOrganizationQuality_Organization_Choice_REJECTED);
      break;
    }
    case TabOrganization::UserChoice::kAccepted: {
      quality_organization->set_choice(
          optimization_guide::proto::
              TabOrganizationQuality_Organization_Choice_ACCEPTED);

      optimization_guide::proto::TabOrganizationQuality_Organization_Label*
          label = quality_organization->mutable_label();
      label->set_edited(organization->names().size() == 0 ||
                        organization->names()[0] !=
                            organization->GetDisplayName());

      for (const TabData::TabID removed_tab_id :
           organization->user_removed_tab_ids()) {
        quality_organization->add_removed_tab_ids(removed_tab_id);
      }

      break;
    }
    case TabOrganization::UserChoice::kNoChoice: {
    }
  }
}

void AddSessionDetailsToQuality(
    optimization_guide::proto::TabOrganizationQuality* quality,
    const TabOrganizationSession* session) {
  CHECK(session && session->request() && session->request()->response());

  if (base::FeatureList::IsEnabled(features::kMultiTabOrganization)) {
    quality->set_user_feedback(session->feedback());
  }

  for (const auto& response_organization :
       session->request()->response()->organizations) {
    std::optional<TabOrganization::ID> response_organization_id =
        response_organization.organization_id;
    TabOrganization* matching_organization_ptr = nullptr;

    if (response_organization_id.has_value()) {
      const auto matching_organization = std::find_if(
          session->tab_organizations().begin(),
          session->tab_organizations().end(),
          [response_organization_id](
              const std::unique_ptr<TabOrganization>& check_organization) {
            return check_organization->organization_id() ==
                   response_organization_id;
          });
      if (matching_organization != session->tab_organizations().end() &&
          matching_organization->get()->choice() !=
              TabOrganization::UserChoice::kNoChoice) {
        matching_organization_ptr = matching_organization->get();
      }
    }

    AddOrganizationDetailsToQualityOrganization(
        quality, matching_organization_ptr, &response_organization);
  }
}
