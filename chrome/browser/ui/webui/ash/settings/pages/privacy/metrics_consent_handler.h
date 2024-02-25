// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_METRICS_CONSENT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_METRICS_CONSENT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_manager.h"

namespace ash::settings {

class TestMetricsConsentHandler;

// Handler for fetching and updating metrics consent.
class MetricsConsentHandler : public ::settings::SettingsPageUIHandler {
 public:
  // Message names sent to WebUI for handling metric consent.
  static const char kGetMetricsConsentState[];
  static const char kUpdateMetricsConsent[];

  MetricsConsentHandler(Profile* profile,
                        metrics::MetricsService* metrics_service,
                        user_manager::UserManager* user_manager);

  MetricsConsentHandler(const MetricsConsentHandler&) = delete;
  MetricsConsentHandler& operator=(const MetricsConsentHandler&) = delete;

  ~MetricsConsentHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class TestMetricsConsentHandler;

  // Handles updating metrics consent for the user.
  void HandleUpdateMetricsConsent(const base::Value::List& args);

  // Handles fetching metrics consent state. The callback will return two
  // values: a string pref name and a boolean indicating whether the current
  // user may change that pref.
  void HandleGetMetricsConsentState(const base::Value::List& args);

  // Returns true if user with |profile_| has permissions to change the metrics
  // consent pref.
  bool IsMetricsConsentConfigurable() const;

  // Returns true if the user metrics consent should be used rather than the
  // device metrics consent.
  bool ShouldUseUserConsent() const;

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<metrics::MetricsService, DanglingUntriaged> metrics_service_;
  const raw_ptr<user_manager::UserManager, DanglingUntriaged> user_manager_;

  // Used for callbacks.
  base::WeakPtrFactory<MetricsConsentHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_METRICS_CONSENT_HANDLER_H_
