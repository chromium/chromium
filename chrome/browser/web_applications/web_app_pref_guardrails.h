// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREF_GUARDRAILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREF_GUARDRAILS_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace web_app {

// WebAppPrefGuardrails provide a simple way of building guardrails based on the
// number of times a prompt on an app has been ignored or dismissed in the past.
// The guardrails help prevent the prompt from showing up after a specific
// number of times based on the user behavior. Data for computing these
// guardrails are stored in the prefs.
class WebAppPrefGuardrails {
 public:
  // Returns an instance of the WebAppPrefGuardrails built to handle when the
  // IPH bubble for the desktop install prompt should be shown.
  static WebAppPrefGuardrails GetForDesktopInstallIph(
      PrefService* pref_service);

  ~WebAppPrefGuardrails();

  // Record that the prompt on app corresponding to app_id being ignored at a
  // specific time.
  void RecordIgnore(const webapps::AppId& app_id, base::Time time);
  // Record that the prompt on app corresponding to app_id being dismissed at a
  // specific time.
  void RecordDismiss(const webapps::AppId& app_id, base::Time time);
  // Record that the prompt on app corresponding to app_id is accepted.
  void RecordAccept(const webapps::AppId& app_id);
  // Returns whether a new prompt should be shown or not for app_id based on
  // values stored in the pref_names_.
  bool IsBlockedByGuardrails(const webapps::AppId& app_id);

 private:
  WebAppPrefGuardrails(PrefService* profile,
                       const GuardrailData& guardrail_data,
                       const GuardrailPrefNames& guardrail_pref_names);
  WebAppPrefGuardrails(const WebAppPrefGuardrails& web_app);
  bool HasIgnoreGuardrails();
  bool HasDismissGuardrails();
  bool ComputeBlockedDueToIgnoreCounts(const webapps::AppId& app_id);

  void UpdateAppSpecificNotAcceptedPrefs(const webapps::AppId& app_id,
                                         base::Time time,
                                         std::string_view time_path);
  void UpdateAppAgnosticNotAcceptedPrefs(base::Time time,
                                         std::string_view time_path);

  raw_ptr<PrefService> pref_service_;
  const raw_ref<const GuardrailData> guardrail_data_;
  const raw_ref<const GuardrailPrefNames> pref_names_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PREF_GUARDRAILS_H_
