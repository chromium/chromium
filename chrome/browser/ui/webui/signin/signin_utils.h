// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_

#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"

struct AccountInfo;
class Browser;
class Profile;
struct AccountInfo;

namespace content {
class RenderFrameHost;
class WebContents;
class WebUI;
}

namespace extensions {
class WebViewGuest;
}

namespace signin {

// User choice when signing in.
// Used for UMA histograms, Hence, constants should never be deleted or
// reordered, and  new constants should only be appended at the end.
// Keep this in sync with SigninChoice in histograms.xml.
enum SigninChoice {
  SIGNIN_CHOICE_CANCEL = 0,       // Signin is cancelled.
  SIGNIN_CHOICE_CONTINUE = 1,     // Signin continues in the current profile.
  SIGNIN_CHOICE_NEW_PROFILE = 2,  // Signin continues in a new profile.
  // SIGNIN_CHOICE_SIZE should always be last.
  SIGNIN_CHOICE_SIZE,
};

// Result of the operation done after the user choice.
enum SigninChoiceOperationResult {
  SIGNIN_TIMEOUT = 0,
  SIGNIN_SILENT_SUCCESS = 1,
  SIGNIN_ERROR = 2,
  SIGNIN_CONFIRM_SUCCESS = 3
};

// Callback with the signin choice and a handler for when the choice has been
// handled.
using SigninChoiceOperationDoneCallback =
    base::OnceCallback<void(SigninChoiceOperationResult)>;
using SigninChoiceWithConfirmationCallback =
    base::OnceCallback<void(SigninChoice, SigninChoiceOperationDoneCallback)>;
using SigninChoiceCallback = base::OnceCallback<void(SigninChoice)>;
using SigninChoiceCallbackVariant =
    std::variant<SigninChoiceCallback,
                 signin::SigninChoiceWithConfirmationCallback>;

struct EnterpriseProfileCreationDialogParams {
  EnterpriseProfileCreationDialogParams(
      AccountInfo account_info,
      bool is_oidc_account,
      bool profile_creation_required_by_policy,
      bool show_link_data_option,
      SigninChoiceCallbackVariant process_user_choice_callback,
      base::OnceClosure done_callback,
      base::OnceClosure retry_callback = base::DoNothing());
  ~EnterpriseProfileCreationDialogParams();
  EnterpriseProfileCreationDialogParams(
      const EnterpriseProfileCreationDialogParams&) = delete;
  EnterpriseProfileCreationDialogParams& operator=(
      const EnterpriseProfileCreationDialogParams&) = delete;

  AccountInfo account_info;
  bool is_oidc_account;
  bool profile_creation_required_by_policy;
  bool show_link_data_option;
  SigninChoiceCallbackVariant process_user_choice_callback;
  base::OnceClosure done_callback;
  base::OnceClosure retry_callback;
};

// Gets a webview within an auth page that has the specified parent frame name
// (i.e. <webview name="foobar"></webview>).
content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name);

extensions::WebViewGuest* GetAuthWebViewGuest(
    content::WebContents* web_contents,
    const std::string& parent_frame_name);

// Gets the browser containing the web UI; if none is found, returns the last
// active browser for web UI's profile.
Browser* GetDesktopBrowser(content::WebUI* web_ui);

// After this time delta, user must see a screen. If it was impossible to get
// the CanShowHistorySyncOptInsWithoutMinorModeRestrictions capability before
// the deadline, the screen should be configured in minor-safe way.
base::TimeDelta GetMinorModeRestrictionsDeadline();

// Sets the height of the WebUI modal dialog after its initialization. This is
// needed to better accomodate different locales' text heights.
void SetInitializedModalHeight(Browser* browser,
                               content::WebUI* web_ui,
                               const base::Value::List& args);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Helps clear Profile info, mainly for managed accounts.
// Idealy this function should not be used much, consider deleting the profile
// if possible instead.
// Undoing the management is hacky (because the management may have installed
// extensions for example).
// TODO(crbug.com/40067597): Remove this function when the FRE is
// adapted.
void ClearProfileWithManagedAccounts(Profile* profile);
#endif

// Gets the account picture in the `account_info` as a data:// URL or the
// default placeholder if it doesn't exist.
std::string GetAccountPictureUrl(const AccountInfo& account_info);

}  // namespace signin

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_
