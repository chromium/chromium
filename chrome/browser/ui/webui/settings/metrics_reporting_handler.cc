// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)

#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_pref_names.h"
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
  pref_member_->Init(
      metrics::prefs::kMetricsReportingEnabled,
      g_browser_process->local_state(),
      base::BindRepeating(&MetricsReportingHandler::OnPrefChanged,
                          base::Unretained(this)));
}

void MetricsReportingHandler::OnJavascriptDisallowed() {
  pref_member_.reset();
}

void MetricsReportingHandler::HandleGetMetricsReporting(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_GT(args.size(), 0u);
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, CreateMetricsReportingDict());
}

base::Value::Dict MetricsReportingHandler::CreateMetricsReportingDict() {
  base::Value::Dict dict;
  dict.Set("enabled",
           ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
  dict.Set("managed", IsMetricsReportingPolicyManaged());
  return dict;
}

void MetricsReportingHandler::HandleSetMetricsReportingEnabled(
    const base::Value::List& args) {
  if (IsMetricsReportingPolicyManaged()) {
    // NOTE: ChangeMetricsReportingState() already checks whether metrics
    // reporting is managed by policy. Also, the UI really shouldn't be able to
    // send this message when managed.
    NOTREACHED();
  }

  bool enabled = args[0].GetBool();
  ChangeMetricsReportingState(
      enabled, ChangeMetricsReportingStateCalledFrom::kUiSettings);
}

void MetricsReportingHandler::OnPrefChanged(const std::string& pref_name) {
  DCHECK_EQ(metrics::prefs::kMetricsReportingEnabled, pref_name);
  SendMetricsReportingChange();
}

void MetricsReportingHandler::SendMetricsReportingChange() {
  FireWebUIListener("metrics-reporting-change", CreateMetricsReportingDict());
}

}  // namespace settings

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
