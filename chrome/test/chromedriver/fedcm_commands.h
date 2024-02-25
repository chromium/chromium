// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_FEDCM_COMMANDS_H_
#define CHROME_TEST_CHROMEDRIVER_FEDCM_COMMANDS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/values.h"

struct Session;
class Status;
class Timeout;
class WebView;

Status ExecuteCancelDialog(Session* session,
                           WebView* web_view,
                           const base::Value::Dict& params,
                           std::unique_ptr<base::Value>* value,
                           Timeout* timeout);

Status ExecuteSelectAccount(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout);

Status ExecuteClickDialogButton(Session* session,
                                WebView* web_view,
                                const base::Value::Dict& params,
                                std::unique_ptr<base::Value>* value,
                                Timeout* timeout);

Status ExecuteGetAccounts(Session* session,
                          WebView* web_view,
                          const base::Value::Dict& params,
                          std::unique_ptr<base::Value>* value,
                          Timeout* timeout);

Status ExecuteGetFedCmTitle(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout);

Status ExecuteGetDialogType(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout);

Status ExecuteSetDelayEnabled(Session* session,
                              WebView* web_view,
                              const base::Value::Dict& params,
                              std::unique_ptr<base::Value>* value,
                              Timeout* timeout);

Status ExecuteResetCooldown(Session* session,
                            WebView* web_view,
                            const base::Value::Dict& params,
                            std::unique_ptr<base::Value>* value,
                            Timeout* timeout);

#endif  // CHROME_TEST_CHROMEDRIVER_FEDCM_COMMANDS_H_
