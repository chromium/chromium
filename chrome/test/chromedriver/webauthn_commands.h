// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_WEBAUTHN_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_WEBAUTHN_COMMANDS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/values.h"

struct Session;
class Status;
class WebView;

using WebAuthnCommand =
    base::RepeatingCallback<Status(WebView* web_view,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value)>;

// Executes a WebAuthn command.
Status ExecuteWebAuthnCommand(const WebAuthnCommand& command,
                              Session* session,
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value);

// Add a virtual authenticator.
Status ExecuteAddVirtualAuthenticator(WebView* web_view,
                                      const base::Value::Dict& params,
                                      std::unique_ptr<base::Value>* value);

// Remove a virtual authenticator.
Status ExecuteRemoveVirtualAuthenticator(WebView* web_view,
                                         const base::Value::Dict& params,
                                         std::unique_ptr<base::Value>* value);

// Inject a credential into an authenticator.
Status ExecuteAddCredential(WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value);

// Retrieve all the credentials stored in an authenticator.
Status ExecuteGetCredentials(WebView* web_view,
                             const base::Value::Dict& params,
                             std::unique_ptr<base::Value>* value);

// Remove a single credential stored in an authenticator.
Status ExecuteRemoveCredential(WebView* web_view,
                               const base::Value::Dict& params,
                               std::unique_ptr<base::Value>* value);

// Remove all the credentials stored in an authenticator.
Status ExecuteRemoveAllCredentials(WebView* web_view,
                                   const base::Value::Dict& params,
                                   std::unique_ptr<base::Value>* value);

// Set whether user verification will succeed or fail on authentication requests
// for the authenticator.
Status ExecuteSetUserVerified(WebView* web_view,
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value);

// Sets a credential's properties.
Status ExecuteSetCredentialProperties(WebView* web_view,
                                      const base::Value::Dict& params,
                                      std::unique_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_WEBAUTHN_COMMANDS_H_
