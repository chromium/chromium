// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EDU_COEXISTENCE_EDU_COEXISTENCE_STATE_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EDU_COEXISTENCE_EDU_COEXISTENCE_STATE_TRACKER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

// A tracker used for the communication between
// `EduCoexistenceChildSigninHelper` and `EduCoexistenceLoginHandler` to allow
// the SigninHelper to wait until parental consent is logged before upserting
// the EDU account into AccountManager.
// A tracker used to record metrics for each educoexistence flow attempt.
class EduCoexistenceStateTracker {
 public:
  // The flow state used to record the status of the flow when it is closed. The
  // enum values are recorded in histogram therefore, keep it consistent with
  // FlowResult enum in enums.xml.
  enum class FlowResult : int {
    // State when the webui first launches.
    kLaunched = 0,

    // Received just before gaia login, and after the parental reauth if
    // applicable
    kConsentValid = 1,

    // The user has entered the correct passwords for edu account and gaia
    // authorizes the account.
    kAccountAuthorized = 2,

    // The parental consent has successfully been logged.
    kConsentLogged = 3,

    // The edu account has successfully been added to AccountManager.
    kAccountAdded = 4,

    // An error has occurred and the error screen is showing.
    kError = 5,

    kNumStates = 6
  };

  struct FlowState {
    FlowState();
    ~FlowState();

    // Whether `EduCoexistenceLoginHandler` has been notified that parental
    // consent has been logged.
    bool received_consent = false;

    // Whether the educoexistence flow is being used during onboarding or
    // in the user session.
    bool is_onboarding = false;

    // The secondary edu account email being added to the device.
    std::string email;

    // Callback to notify `EduCoexistenceChildSigninHelper` to upsert the
    // account into account manager when parental consent is received.
    base::OnceCallback<void(bool)> consent_logged_callback;

    // Enum tracking the state of the EduCoexistence flow.
    FlowResult flow_result = FlowResult::kLaunched;
  };

  static EduCoexistenceStateTracker* Get();

  // Called from `EduCoexistenceChildSigninHelper` to wait for parental consent
  // logged signal. `consent_logged_callback` is executed when the signal is
  // received.
  void SetEduConsentCallback(
      const content::WebUI* web_ui,
      const std::string& account_email,
      base::OnceCallback<void(bool)> consent_logged_callback);

  // Called when `EduCoexistenceLoginHandler` is being destroyed. If the dialog
  // is closed while there is still a callback for
  // `EduCoexistenceChildSigninHelper` waiting to be executed, this executes
  // the `consent_logged_callback` with success=false argument.
  void OnDialogClosed(const content::WebUI* web_ui);

  // Called with `EduCoexistenceLoginHandler` is created and has access to its
  // `web_ui`.`is_onboarding` is set to true if the dialog is being shown in
  // OOBE or in an add person flow.
  void OnDialogCreated(const content::WebUI* web_ui, bool is_onboarding);

  // Called when the parental consent signal has been logged.
  void OnConsentLogged(const content::WebUI* web_ui,
                       const std::string& account_email);

  // Called to record the progress of the EduCoexistence flow.
  void OnWebUiStateChanged(const content::WebUI* web_ui, FlowResult result);

  const FlowState* GetInfoForWebUIForTest(const content::WebUI* web_ui) const;
  std::string GetInSessionHistogramNameForTest() const;

 private:
  friend class base::NoDestructor<EduCoexistenceStateTracker>;

  EduCoexistenceStateTracker();
  EduCoexistenceStateTracker(const EduCoexistenceStateTracker&) = delete;
  EduCoexistenceStateTracker& operator=(const EduCoexistenceStateTracker&) =
      delete;
  ~EduCoexistenceStateTracker();

  // Maps each flow's WebUI's to the corresponding FlowState. While there an
  // only be one dialog for the EduCoexistence flow, there can potentially be
  // multiple WebUIs because the user can start the WebUIs from a chrome browser
  // tab by going to chrome://chrome-signin/edu-coexistence.
  std::map<const content::WebUI*, FlowState> state_tracker_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EDU_COEXISTENCE_EDU_COEXISTENCE_STATE_TRACKER_H_
