// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_ALERT_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_ALERT_COMMANDS_H_

#include <memory>

#include "base/callback_forward.h"

namespace base {
class DictionaryValue;
class Value;
}

struct Session;
class Status;
class WebView;

typedef base::RepeatingCallback<Status(Session* session,
                                       WebView* web_view,
                                       const base::DictionaryValue&,
                                       std::unique_ptr<base::Value>*)>
    AlertCommand;

// Executes an alert command.
Status ExecuteAlertCommand(const AlertCommand& alert_command,
                           Session* session,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value);

// Returns whether an alert is open.
Status ExecuteGetAlert(Session* session,
                       WebView* web_view,
                       const base::DictionaryValue& params,
                       std::unique_ptr<base::Value>* value);

// Returns the text of the open alert.
Status ExecuteGetAlertText(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value);

// Sets the value of the alert prompt.
Status ExecuteSetAlertText(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value);

// Accepts the open alert.
Status ExecuteAcceptAlert(Session* session,
                          WebView* web_view,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value);

// Dismisses the open alert.
Status ExecuteDismissAlert(Session* session,
                           WebView* web_view,
                           const base::DictionaryValue& params,
                           std::unique_ptr<base::Value>* value);

#endif  // CHROME_TEST_CHROMEDRIVER_ALERT_COMMANDS_H_
