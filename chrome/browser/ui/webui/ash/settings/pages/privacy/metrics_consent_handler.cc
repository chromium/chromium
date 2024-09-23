// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/metrics_consent_handler.h"

#include "base/check.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::settings {

const char MetricsConsentHandler::kGetMetricsConsentState[] =
    "getMetricsConsentState";
const char MetricsConsentHandler::kUpdateMetricsConsent[] =
    "updateMetricsConsent";

MetricsConsentHandler::MetricsConsentHandler(
    Profile* profile,
    metrics::MetricsService* metrics_service,
    user_manager::UserManager* user_manager)
    : profile_(profile),
      metrics_service_(metrics_service),
      user_manager_(user_manager) {
  DCHECK(profile_);
  DCHECK(metrics_service_);
  DCHECK(user_manager_);
}

MetricsConsentHandler::~MetricsConsentHandler() = default;

void MetricsConsentHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kUpdateMetricsConsent,
      base::BindRepeating(&MetricsConsentHandler::HandleUpdateMetricsConsent,
                          weak_ptr_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      kGetMetricsConsentState,
      base::BindRepeating(&MetricsConsentHandler::HandleGetMetricsConsentState,
                          weak_ptr_factory_.GetWeakPtr()));
}

void MetricsConsentHandler::OnJavascriptAllowed() {}

void MetricsConsentHandler::OnJavascriptDisallowed() {}

void MetricsConsentHandler::HandleGetMetricsConsentState(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const base::Value& callback_id = args[0];

  base::Value::Dict response;

  base::Value consent_pref =
      ShouldUseUserConsent()
          ? base::Value(::metrics::prefs::kMetricsUserConsent)
          : base::Value(kStatsReportingPref);

  response.Set("prefName", std::move(consent_pref));
  response.Set("isConfigurable", base::Value(IsMetricsConsentConfigurable()));

  ResolveJavascriptCallback(callback_id, response);
}

void MetricsConsentHandler::HandleUpdateMetricsConsent(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  CHECK_EQ(args[1].type(), base::Value::Type::DICT);

  const base::Value& callback_id = args[0];
  std::optional<bool> metrics_consent = args[1].GetDict().FindBool("consent");
  CHECK(metrics_consent);

  if (!ShouldUseUserConsent()) {
    auto* stats_reporting_controller = StatsReportingController::Get();
    stats_reporting_controller->SetEnabled(profile_, *metrics_consent);

    // Re-read from |stats_reporting_controller|. If |profile_| is not owner,
    // then the consent should not have changed to |metrics_consent|.
    ResolveJavascriptCallback(
        callback_id, base::Value(stats_reporting_controller->IsEnabled()));
    return;
  }

  metrics_service_->UpdateCurrentUserMetricsConsent(*metrics_consent);
  std::optional<bool> user_metrics_consent =
      metrics_service_->GetCurrentUserMetricsConsent();
  CHECK(user_metrics_consent.has_value());
  ResolveJavascriptCallback(callback_id, base::Value(*user_metrics_consent));
}

bool MetricsConsentHandler::IsMetricsConsentConfigurable() const {
  // TODO(b/333911538): In the interim, completely disable child users
  // from being able to toggle consent in the settings. Once the parent sets
  // the consent for the child during OOBE, it cannot be updated afterwards.
  if (user_manager_->IsLoggedInAsChildUser()) {
    return false;
  }

  return ShouldUseUserConsent() || user_manager_->IsCurrentUserOwner();
}

bool MetricsConsentHandler::ShouldUseUserConsent() const {
  return metrics_service_->GetCurrentUserMetricsConsent().has_value();
}

}  // namespace ash::settings
