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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"  // nogncheck
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // To match the pre-Lacros settings UX, we show the managed icon if the ash
  // device-level metrics reporting pref is managed. https://crbug.com/1148604
  bool managed = chromeos::BrowserParamsProxy::Get()->AshMetricsManaged() ==
                 crosapi::mojom::MetricsReportingManaged::kManaged;
  dict.Set("managed", managed);
#else
  dict.Set("managed", IsMetricsReportingPolicyManaged());
#endif
  return dict;
}

void MetricsReportingHandler::HandleSetMetricsReportingEnabled(
    const base::Value::List& args) {
  if (IsMetricsReportingPolicyManaged()) {
    NOTREACHED_IN_MIGRATION();
    // NOTE: ChangeMetricsReportingState() already checks whether metrics
    // reporting is managed by policy. Also, the UI really shouldn't be able to
    // send this message when managed. However, in this specific case, there's a
    // sane, graceful fallback so we might as well do that.
    SendMetricsReportingChange();
    return;
  }

  bool enabled = args[0].GetBool();
  ChangeMetricsReportingState(
      enabled, ChangeMetricsReportingStateCalledFrom::kUiSettings);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // To match the pre-Lacros settings UX, the metrics reporting toggle in Lacros
  // browser settings controls both browser metrics reporting and OS metrics
  // reporting. See https://crbug.com/1148604.
  auto* lacros_chrome_service = chromeos::LacrosService::Get();
  // Service may be null in tests.
  if (!lacros_chrome_service)
    return;
  // The metrics reporting API was added in Chrome OS 89.
  if (!lacros_chrome_service->IsSupported<crosapi::mojom::MetricsReporting>()) {
    LOG(WARNING) << "MetricsReporting API not available";
    return;
  }
  // Bind the remote here instead of the constructor because this function
  // is rarely called, so we usually don't need the remote.
  if (!metrics_reporting_remote_.is_bound()) {
    lacros_chrome_service->BindMetricsReporting(
        metrics_reporting_remote_.BindNewPipeAndPassReceiver());
  }
  // Set metrics reporting state in ash-chrome.
  metrics_reporting_remote_->SetMetricsReportingEnabled(enabled,
                                                        base::DoNothing());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
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
