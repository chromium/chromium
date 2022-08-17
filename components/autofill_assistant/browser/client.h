// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
class PersonalDataManager;
}  // namespace autofill

namespace password_manager {
class PasswordChangeSuccessTracker;
}  // namespace password_manager

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace autofill_assistant {
class AccessTokenFetcher;
class WebsiteLoginManager;

// A client interface that needs to be supplied to the controller by the
// embedder.
class Client {
 public:
  virtual ~Client() = default;

  // Attaches the controller to the UI.
  //
  // This method does whatever is necessary to guarantee that, at the end of the
  // call, there is a Controller associated with a UI.
  virtual void AttachUI() = 0;

  // Posts a task to destroy the UI.
  virtual void DestroyUISoon() = 0;

  // Destroys the UI immediately.
  virtual void DestroyUI() = 0;

  // Returns the channel for the installation (canary, dev, beta, stable).
  // Returns unknown otherwise.
  virtual version_info::Channel GetChannel() const = 0;

  // Returns the e-mail address that corresponds to the auth credentials. Might
  // be empty.
  virtual std::string GetEmailAddressForAccessTokenAccount() const = 0;

  // Returns the e-mail address used to sign into Chrome, or an empty string if
  // the user is not signed in.
  virtual std::string GetSignedInEmail() const = 0;

  // Returns the AccessTokenFetcher to use to get oauth credentials.
  virtual AccessTokenFetcher* GetAccessTokenFetcher() = 0;

  // Returns the current active personal data manager.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() const = 0;

  // Returns the currently active login manager.
  virtual WebsiteLoginManager* GetWebsiteLoginManager() const = 0;

  // Returns the current password change success tracker.
  virtual password_manager::PasswordChangeSuccessTracker*
  GetPasswordChangeSuccessTracker() const = 0;

  // Returns the locale.
  virtual std::string GetLocale() const = 0;

  // Returns the country code.
  virtual std::string GetCountryCode() const = 0;

  // Returns details about the device.
  virtual DeviceContext GetDeviceContext() const = 0;

  // Returns whether a11y (talkback and touch exploration) is enabled or not.
  virtual bool IsAccessibilityEnabled() const = 0;

  // Returns whether an accessibility service with "FEEDBACK_SPOKEN" feedback
  // type is enabled or not.
  virtual bool IsSpokenFeedbackAccessibilityServiceEnabled() const = 0;

  // Returns the width and height of the window.
  virtual absl::optional<std::pair<int, int>> GetWindowSize() const = 0;

  // Returns the orientation of the screen.
  virtual ClientContextProto::ScreenOrientation GetScreenOrientation()
      const = 0;

  // Returns the payments client token through the |callback|.
  virtual void FetchPaymentsClientToken(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Returns current WebContents.
  virtual content::WebContents* GetWebContents() const = 0;

  // Stops autofill assistant for the current WebContents, both controller
  // and UI.
  // The reason is ignored if RecordDropOut has been previously called.
  virtual void Shutdown(Metrics::DropOutReason reason) = 0;

  // Records the reason of the drop out. Any subsequent reason for the current
  // run will be ignored.
  virtual void RecordDropOut(Metrics::DropOutReason reason) = 0;

  // Whether this client has had an UI.
  virtual bool HasHadUI() const = 0;

  // Returns the ScriptExecutorUiDelegate if it exists, otherwise returns
  // nullptr.
  virtual ScriptExecutorUiDelegate* GetScriptExecutorUiDelegate() = 0;

  // Returns whether or not this instance of Autofill Assistant must use a
  // backend endpoint to query data.
  virtual bool MustUseBackendData() const = 0;

  // Return the annotate DOM model version, if available.
  virtual void GetAnnotateDomModelVersion(
      base::OnceCallback<void(absl::optional<int64_t>)> callback) const = 0;

  // Checks if given XML is signed or not.
  virtual bool IsXmlSigned(const std::string& xml_string) const = 0;

  // Extracts attribute values from the |xml_string| corresponding to the
  // |keys|.
  virtual const std::vector<std::string> ExtractValuesFromSingleTagXml(
      const std::string& xml_string,
      const std::vector<std::string>& keys) const = 0;

  // Return whether MSBB is enabled.
  virtual bool GetMakeSearchesAndBrowsingBetterEnabled() const = 0;

  // Return whether metrics reporting is enable.
  virtual bool GetMetricsReportingEnabled() const = 0;

 protected:
  Client() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_H_
