// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_METRICS_REPORTING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_METRICS_REPORTING_HANDLER_H_

#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)

#include <memory>

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_member.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"  // nogncheck
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace settings {

class MetricsReportingHandler : public SettingsPageUIHandler {
 public:
  MetricsReportingHandler();

  MetricsReportingHandler(const MetricsReportingHandler&) = delete;
  MetricsReportingHandler& operator=(const MetricsReportingHandler&) = delete;

  ~MetricsReportingHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Handler for "getMetricsReporting" message. No arguments. Protected for
  // testing.
  void HandleGetMetricsReporting(const base::Value::List& args);

 private:
  // Describes the state of metrics reporting in a `base::Value::Dict`.
  // Friends with ChromeMetricsServiceAccessor.
  base::Value::Dict CreateMetricsReportingDict();

  // Handler for "setMetricsReportingEnabled" message. Passed a single,
  // |enabled| boolean argument.
  void HandleSetMetricsReportingEnabled(const base::Value::List& args);

  // Called when the local state pref controlling metrics reporting changes.
  void OnPrefChanged(const std::string& pref_name);

  // Sends a "metrics-reporting-change" WebUI listener event to the page.
  void SendMetricsReportingChange();

  // Used to track pref changes that affect whether metrics reporting is
  // enabled.
  std::unique_ptr<BooleanPrefMember> pref_member_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The metrics reporting interface in ash-chrome.
  mojo::Remote<crosapi::mojom::MetricsReporting> metrics_reporting_remote_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace settings

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_METRICS_REPORTING_HANDLER_H_
