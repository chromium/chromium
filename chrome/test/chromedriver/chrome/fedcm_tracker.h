// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_FEDCM_TRACKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_FEDCM_TRACKER_H_

#include <string>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

// Tracks the FedCM dialog events.
class FedCmTracker : public DevToolsEventListener {
 public:
  explicit FedCmTracker(DevToolsClient* client);

  FedCmTracker(const FedCmTracker&) = delete;
  FedCmTracker& operator=(const FedCmTracker&) = delete;

  ~FedCmTracker() override;

  // Enables the FedCM events.
  Status Enable(DevToolsClient* client);

  // DevToolsEventListener:
  bool ListensToConnections() const override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

  bool HasDialog() const { return !last_dialog_id_.empty(); }

  const std::string& GetLastDialogId() const { return last_dialog_id_; }

  const base::Value::List& GetLastAccounts() const { return last_accounts_; }

  const std::string& GetLastTitle() const { return last_title_; }
  const std::optional<std::string>& GetLastSubtitle() const {
    return last_subtitle_;
  }

  const std::string& GetLastDialogType() const { return last_dialog_type_; }

  // To be called when the client issues one of the commands that
  // close the dialog or if we get a dialogClosed event.
  void DialogClosed() {
    last_dialog_id_ = "";
    last_title_ = "";
    last_subtitle_ = "";
    last_dialog_type_ = "";
    last_accounts_ = base::Value::List();
  }

 private:
  std::string last_dialog_id_;
  std::string last_title_;
  std::optional<std::string> last_subtitle_;
  std::string last_dialog_type_;
  base::Value::List last_accounts_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_FEDCM_TRACKER_H_
