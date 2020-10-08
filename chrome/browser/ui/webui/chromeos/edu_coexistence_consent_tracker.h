// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_COEXISTENCE_CONSENT_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_COEXISTENCE_CONSENT_TRACKER_H_

#include <map>
#include <string>

#include "base/callback.h"

namespace content {
class WebUI;
}  // namespace content

namespace chromeos {

// A tracker used for the communication between
// |EduCoexistenceChildSigninHelper| and |EduCoexistenceLoginHandler| to allow
// the SigninHelper to wait until parental consent is logged before upserting
// the EDU account into AccountManager.
class EduCoexistenceConsentTracker {
 public:
  struct EmailAndCallback {
    EmailAndCallback();
    ~EmailAndCallback();

    bool received_consent = false;
    std::string email;
    base::OnceCallback<void(bool)> callback;
  };

  static EduCoexistenceConsentTracker* Get();

  // Don't try to instantiate this class. Use the static getter instead.
  EduCoexistenceConsentTracker();
  EduCoexistenceConsentTracker(const EduCoexistenceConsentTracker&) = delete;
  EduCoexistenceConsentTracker& operator=(const EduCoexistenceConsentTracker&) =
      delete;
  ~EduCoexistenceConsentTracker();

  // Called from |EduCoexistenceChildSigninHelper| to wait for parental consent
  // logged signal. |callback| is executed when the signal is received.
  void WaitForEduConsent(const content::WebUI* web_ui,
                         const std::string& account_email,
                         base::OnceCallback<void(bool)> callback);

  // Called when |EduCoexistenceLoginHandler| is being destroyed. If the dialog
  // is closed while there is still a |EduCoexistenceChildSigninHelper| callback
  // waiting to be executed, this executes the callback with success=false
  // argument.
  void OnDialogClosed(const content::WebUI* web_ui);

  // Called from |EduCoexistenceLoginHandler| when the parental consent signal
  // has been logged.
  void OnConsentLogged(const content::WebUI* web_ui,
                       const std::string& account_email);

  const EmailAndCallback* GetInfoForWebUIForTest(const content::WebUI* web_ui);

 private:
  std::map<const content::WebUI*, EmailAndCallback> consent_tracker_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EDU_COEXISTENCE_CONSENT_TRACKER_H_
