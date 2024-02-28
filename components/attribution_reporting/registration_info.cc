// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/registration_info.h"

#include <optional>

#include "base/containers/enum_set.h"
#include "base/functional/function_ref.h"
#include "components/attribution_reporting/registrar.h"
#include "services/network/public/cpp/attribution_utils.h"
#include "services/network/public/mojom/attribution.mojom-shared.h"

namespace attribution_reporting {

namespace {

using SetRegistrarOrIssueFunc = base::FunctionRef<
    void(bool, network::mojom::AttributionSupport, RegistrationInfo&)>;

void SetRegistrarOrIssue(
    bool is_source,
    network::mojom::AttributionSupport support,
    Registrar registrar,
    IssueType source_issue,
    IssueType trigger_issue,
    base::FunctionRef<bool(network::mojom::AttributionSupport)> check_func,
    RegistrationInfo& info) {
  if (check_func(support)) {
    info.registrar = registrar;
  } else {
    info.issues.Put(is_source ? source_issue : trigger_issue);
  }
}

void SetWebRegistrarOrIssue(bool is_source,
                            network::mojom::AttributionSupport support,
                            RegistrationInfo& info) {
  SetRegistrarOrIssue(is_source, support, Registrar::kWeb,
                      IssueType::kSourceIgnored, IssueType::kTriggerIgnored,
                      &network::HasAttributionWebSupport, info);
}

void SetOsRegistrarOrIssue(bool is_source,
                           network::mojom::AttributionSupport support,
                           RegistrationInfo& info) {
  SetRegistrarOrIssue(is_source, support, Registrar::kOs,
                      IssueType::kOsSourceIgnored, IssueType::kOsTriggerIgnored,
                      &network::HasAttributionOsSupport, info);
}

void HandlePreferredPlatform(bool is_source,
                             network::mojom::AttributionSupport support,
                             IssueType source_issue,
                             IssueType trigger_issue,
                             bool has_preferred_header,
                             SetRegistrarOrIssueFunc preferred_func,
                             bool has_secondary_header,
                             SetRegistrarOrIssueFunc secondary_func,
                             RegistrationInfo& info) {
  if (!has_preferred_header) {
    info.issues.Put(is_source ? source_issue : trigger_issue);
    return;
  }

  preferred_func(is_source, support, info);

  if (!info.registrar.has_value() && has_secondary_header) {
    secondary_func(is_source, support, info);
  }
}

}  // namespace

RegistrationInfo::RegistrationInfo() = default;

RegistrationInfo::~RegistrationInfo() = default;

RegistrationInfo::RegistrationInfo(const RegistrationInfo&) = default;

RegistrationInfo& RegistrationInfo::operator=(const RegistrationInfo&) =
    default;

RegistrationInfo::RegistrationInfo(RegistrationInfo&&) = default;

RegistrationInfo& RegistrationInfo::operator=(RegistrationInfo&&) = default;

// static
RegistrationInfo RegistrationInfo::Get(
    bool has_web_header,
    bool has_os_header,
    bool is_source,
    std::optional<Registrar> preferred_platform,
    network::mojom::AttributionSupport support) {
  if (!has_web_header && !has_os_header) {
    return RegistrationInfo();
  }

  RegistrationInfo info;

  if (!preferred_platform.has_value()) {
    if (has_web_header && has_os_header) {
      info.issues.Put(IssueType::kWebAndOsHeaders);
    } else if (has_web_header) {
      SetWebRegistrarOrIssue(is_source, support, info);
    } else if (has_os_header) {
      SetOsRegistrarOrIssue(is_source, support, info);
    }
    return info;
  }

  switch (preferred_platform.value()) {
    case attribution_reporting::Registrar::kWeb:
      HandlePreferredPlatform(
          is_source, support, IssueType::kNoRegisterSourceHeader,
          IssueType::kNoRegisterTriggerHeader, has_web_header,
          &SetWebRegistrarOrIssue, has_os_header, &SetOsRegistrarOrIssue, info);
      break;
    case attribution_reporting::Registrar::kOs:
      HandlePreferredPlatform(is_source, support,
                              IssueType::kNoRegisterOsSourceHeader,
                              IssueType::kNoRegisterOsTriggerHeader,
                              has_os_header, &SetOsRegistrarOrIssue,
                              has_web_header, &SetWebRegistrarOrIssue, info);
      break;
  }

  return info;
}

}  // namespace attribution_reporting
