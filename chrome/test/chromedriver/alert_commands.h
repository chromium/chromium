// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_ALERT_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_ALERT_COMMANDS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/values.h"

struct Session;
class Status;
class WebView;

typedef base::RepeatingCallback<Status(Session* session,
                                       WebView* web_view,
                                       const base::Value::Dict&,
                                       std::unique_ptr<base::Value>*)>
    AlertCommand;

// Executes an alert command.
Status ExecuteAlertCommand(const AlertCommand& alert_command,
                           Session* session,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value);

// Returns whether an alert is open.
Status ExecuteGetAlert(Session* session,
                       WebView* web_view,
                       const base::Value::Dict& params,
                       std::unique_ptr<base::Value>* value);

// Returns the text of the open alert.
Status ExecuteGetAlertText(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value);

// Sets the value of the alert prompt.
Status ExecuteSetAlertText(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value);

// Accepts the open alert.
Status ExecuteAcceptAlert(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value);

// Dismisses the open alert.
Status ExecuteDismissAlert(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_ALERT_COMMANDS_H_
