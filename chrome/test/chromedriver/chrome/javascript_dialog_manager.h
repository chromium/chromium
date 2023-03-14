// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_JAVASCRIPT_DIALOG_MANAGER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_JAVASCRIPT_DIALOG_MANAGER_H_

#include <list>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

struct BrowserInfo;
class DevToolsClient;
class Status;

// Tracks the opening and closing of JavaScript dialogs (e.g., alerts).
class JavaScriptDialogManager : public DevToolsEventListener {
 public:
  explicit JavaScriptDialogManager(DevToolsClient* client,
                                   const BrowserInfo* browser_info);

  JavaScriptDialogManager(const JavaScriptDialogManager&) = delete;
  JavaScriptDialogManager& operator=(const JavaScriptDialogManager&) = delete;

  ~JavaScriptDialogManager() override;

  bool IsDialogOpen() const;

  Status GetDialogMessage(std::string* message);

  Status GetTypeOfDialog(std::string* type);

  Status HandleDialog(bool accept, const std::string* text);

  // Overridden from DevToolsEventListener:
  Status OnConnected(DevToolsClient* client) override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

 private:
  const raw_ptr<DevToolsClient> client_;
  // The queue of unhandled dialogs. This may be greater than 1 in rare
  // cases. E.g., if the page shows an alert but before the manager received
  // the event, a script was injected via Inspector that triggered an alert.
  std::list<std::string> unhandled_dialog_queue_;

  std::list<std::string> dialog_type_queue_;

  std::string prompt_text_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_JAVASCRIPT_DIALOG_MANAGER_H_
