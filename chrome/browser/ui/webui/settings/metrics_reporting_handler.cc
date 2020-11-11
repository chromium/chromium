// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)

#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace settings {

MetricsReportingHandler::MetricsReportingHandler() {}
MetricsReportingHandler::~MetricsReportingHandler() {}

void MetricsReportingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getMetricsReporting",
      base::BindRepeating(&MetricsReportingHandler::HandleGetMetricsReporting,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setMetricsReportingEnabled",
      base::BindRepeating(
          &MetricsReportingHandler::HandleSetMetricsReportingEnabled,
          base::Unretained(this)));
}

void MetricsReportingHandler::OnJavascriptAllowed() {
  pref_member_ = std::make_unique<BooleanPrefMember>();
  pref_member_->Init(metrics::prefs::kMetricsReportingEnabled,
                     g_browser_process->local_state(),
                     base::Bind(&MetricsReportingHandler::OnPrefChanged,
                                base::Unretained(this)));

  policy_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      g_browser_process->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_registrar_->Observe(policy::key::kMetricsReportingEnabled,
      base::Bind(&MetricsReportingHandler::OnPolicyChanged,
                 base::Unretained(this)));
}

void MetricsReportingHandler::OnJavascriptDisallowed() {
  pref_member_.reset();
  policy_registrar_.reset();
}

void MetricsReportingHandler::HandleGetMetricsReporting(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(*callback_id, *CreateMetricsReportingDict());
}

std::unique_ptr<base::DictionaryValue>
    MetricsReportingHandler::CreateMetricsReportingDict() {
  std::unique_ptr<base::DictionaryValue> dict(
      std::make_unique<base::DictionaryValue>());
  dict->SetBoolean(
      "enabled",
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  dict->SetBoolean("managed", IsMetricsReportingPolicyManaged());
  return dict;
}

void MetricsReportingHandler::HandleSetMetricsReportingEnabled(
    const base::ListValue* args) {
  if (IsMetricsReportingPolicyManaged()) {
    NOTREACHED();
    // NOTE: ChangeMetricsReportingState() already checks whether metrics
    // reporting is managed by policy. Also, the UI really shouldn't be able to
    // send this message when managed. However, in this specific case, there's a
    // sane, graceful fallback so we might as well do that.
    SendMetricsReportingChange();
    return;
  }

  bool enabled;
  CHECK(args->GetBoolean(0, &enabled));
  ChangeMetricsReportingState(enabled);
}

void MetricsReportingHandler::OnPolicyChanged(const base::Value* previous,
                                              const base::Value* current) {
  SendMetricsReportingChange();
}

void MetricsReportingHandler::OnPrefChanged(const std::string& pref_name) {
  DCHECK_EQ(metrics::prefs::kMetricsReportingEnabled, pref_name);
  SendMetricsReportingChange();
}

void MetricsReportingHandler::SendMetricsReportingChange() {
  FireWebUIListener("metrics-reporting-change", *CreateMetricsReportingDict());
}

}  // namespace settings

#endif  // defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
