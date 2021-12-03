// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/metrics_consent_handler.h"

#include "ash/components/settings/cros_settings_names.h"
#include "base/check.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace settings {

const char MetricsConsentHandler::kGetMetricsConsentState[] =
    "getMetricsConsentState";
const char MetricsConsentHandler::kUpdateMetricsConsent[] =
    "updateMetricsConsent";

MetricsConsentHandler::MetricsConsentHandler(
    Profile* profile,
    user_manager::UserManager* user_manager)
    : profile_(profile), user_manager_(user_manager) {
  DCHECK(profile_);
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
    base::Value::ConstListView args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const base::Value& callback_id = args[0];

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey("prefName", base::Value(::ash::kStatsReportingPref));
  response.SetKey("isConfigurable",
                  base::Value(IsMetricsConsentConfigurable()));

  ResolveJavascriptCallback(callback_id, response);
}

void MetricsConsentHandler::HandleUpdateMetricsConsent(
    base::Value::ConstListView args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  CHECK_EQ(args[1].type(), base::Value::Type::DICTIONARY);

  const base::Value& callback_id = args[0];
  absl::optional<bool> metrics_consent = args[1].FindBoolKey("consent");
  CHECK(metrics_consent);

  auto* stats_reporting_controller = ash::StatsReportingController::Get();
  stats_reporting_controller->SetEnabled(profile_, metrics_consent.value());

  // Re-read from |stats_reporting_controller|. If |profile_| is not owner, then
  // the consent should not have changed to |metrics_consent|.
  ResolveJavascriptCallback(
      callback_id, base::Value(stats_reporting_controller->IsEnabled()));
}

bool MetricsConsentHandler::IsMetricsConsentConfigurable() const {
  return user_manager_->IsCurrentUserOwner();
}

}  // namespace settings
}  // namespace chromeos
