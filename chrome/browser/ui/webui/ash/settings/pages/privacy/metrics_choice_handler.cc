// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/metrics_choice_handler.h"

#include "base/check.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/metrics/profile_pref_names.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::settings {

const char MetricsChoiceHandler::kGetMetricsChoiceState[] =
    "getMetricsConsentState";
const char MetricsChoiceHandler::kUpdateMetricsChoice[] =
    "updateMetricsConsent";

MetricsChoiceHandler::MetricsChoiceHandler(
    Profile* profile,
    metrics::MetricsService* metrics_service,
    user_manager::UserManager* user_manager)
    : profile_(profile),
      metrics_service_(metrics_service),
      user_manager_(user_manager) {
  CHECK(profile_, base::NotFatalUntil::M162);
  CHECK(metrics_service_, base::NotFatalUntil::M162);
  CHECK(user_manager_, base::NotFatalUntil::M162);
}

MetricsChoiceHandler::~MetricsChoiceHandler() = default;

void MetricsChoiceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kUpdateMetricsChoice,
      base::BindRepeating(&MetricsChoiceHandler::HandleUpdateMetricsChoice,
                          weak_ptr_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      kGetMetricsChoiceState,
      base::BindRepeating(&MetricsChoiceHandler::HandleGetMetricsChoiceState,
                          weak_ptr_factory_.GetWeakPtr()));
}

void MetricsChoiceHandler::OnJavascriptAllowed() {}

void MetricsChoiceHandler::OnJavascriptDisallowed() {}

void MetricsChoiceHandler::HandleGetMetricsChoiceState(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const base::Value& callback_id = args[0];

  base::DictValue response;

  base::Value choice_pref =
      ShouldUseUserChoice() ? base::Value(::metrics::prefs::kMetricsUserConsent)
                            : base::Value(kStatsReportingPref);

  response.Set("prefName", std::move(choice_pref));
  response.Set("isConfigurable", base::Value(IsMetricsChoiceConfigurable()));

  ResolveJavascriptCallback(callback_id, response);
}

void MetricsChoiceHandler::HandleUpdateMetricsChoice(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  CHECK_EQ(args[1].type(), base::Value::Type::DICT);

  const base::Value& callback_id = args[0];
  std::optional<bool> metrics_choice = args[1].GetDict().FindBool("consent");
  CHECK(metrics_choice);

  if (!ShouldUseUserChoice()) {
    auto* stats_reporting_controller = StatsReportingController::Get();
    stats_reporting_controller->SetEnabled(profile_, *metrics_choice);

    // Re-read from |stats_reporting_controller|. If |profile_| is not owner,
    // then the choice should not have changed to |metrics_choice|.
    ResolveJavascriptCallback(
        callback_id, base::Value(stats_reporting_controller->IsEnabled()));
    return;
  }

  metrics_service_->UpdateCurrentUserMetricsChoice(*metrics_choice);
  std::optional<bool> user_choice =
      metrics_service_->GetCurrentUserMetricsChoice();
  CHECK(user_choice.has_value());
  ResolveJavascriptCallback(callback_id, base::Value(*user_choice));
}

bool MetricsChoiceHandler::IsMetricsChoiceConfigurable() const {
  // TODO(b/333911538): In the interim, completely disable child users
  // from being able to toggle choice in the settings. Once the parent sets
  // the choice for the child during OOBE, it cannot be updated afterwards.
  if (user_manager_->IsLoggedInAsChildUser()) {
    return false;
  }

  return ShouldUseUserChoice() || user_manager_->IsCurrentUserOwner();
}

bool MetricsChoiceHandler::ShouldUseUserChoice() const {
  return metrics_service_->GetCurrentUserMetricsChoice().has_value();
}

}  // namespace ash::settings
