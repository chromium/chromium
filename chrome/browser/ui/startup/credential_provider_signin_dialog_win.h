// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_H_
#define CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ui/startup/buildflags.h"

namespace base {
class CommandLine;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace views {
class WebDialogView;
}  // namespace views

// Callback signalled by the dialog when the Gaia sign in flow completes.
// Parameters are:
// 1. A base::Value that is of type DICTIONARY. The dictionary will always
//    contain an exit_code entry and possibly more data if exit_code ==
//    credential_provider::kUiecSuccess.
// 2. Any extra scopes provided through flags.
// 3. A URL loader that will be used by various OAuth fetchers.
using HandleGcpwSigninCompleteResult =
    base::OnceCallback<void(base::Value::Dict,
                            const std::string& additional_mdm_oauth_scopes,
                            scoped_refptr<network::SharedURLLoaderFactory>)>;

// Starts the Google Credential Provider for Windows (GCPW) Sign in flow. First
// the function shows a frameless Google account sign in page allowing the user
// to choose an  account to logon to Windows. Once the signin is complete, the
// flow will automatically start requesting additional information required by
// GCPW to complete Windows logon.
// Returns false if the dialog could not be loaded due to the current execution
// mode.
bool StartGCPWSignin(const base::CommandLine& command_line,
                     content::BrowserContext* context);

// This function displays a dialog window with a Gaia signin page. Once
// the Gaia signin flow is finished, the callback given by
// |signin_complete_handler| will be called with the results of the signin.
// The return value is only valid during the lifetime of the dialog.
views::WebDialogView* ShowCredentialProviderSigninDialog(
    const base::CommandLine& command_line,
    content::BrowserContext* context,
    HandleGcpwSigninCompleteResult signin_complete_handler);

#if BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)
// Allow displaying of GCPW signin dialog when not under the winlogon desktop
// for testing purposes.
void EnableGcpwSigninDialogForTesting(bool enable);
#endif  // BUILDFLAG(CAN_TEST_GCPW_SIGNIN_STARTUP)

#endif  // CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_DIALOG_WIN_H_
