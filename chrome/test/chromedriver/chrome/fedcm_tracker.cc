// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/fedcm_tracker.h"

#include "base/logging.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

FedCmTracker::FedCmTracker(DevToolsClient* client) {
  client->AddListener(this);
}

FedCmTracker::~FedCmTracker() = default;

Status FedCmTracker::Enable(DevToolsClient* client) {
  return client->SendCommand("FedCm.enable", base::Value::Dict());
}

bool FedCmTracker::ListensToConnections() const {
  return false;
}

Status FedCmTracker::OnEvent(DevToolsClient* client,
                             const std::string& method,
                             const base::Value::Dict& params) {
  if (method == "FedCm.dialogClosed") {
    DialogClosed();
    return Status(kOk);
  }

  if (method != "FedCm.dialogShown") {
    return Status(kOk);
  }

  const std::string* id = params.FindString("dialogId");
  last_dialog_id_ = id ? *id : "";
  const std::string* str = params.FindString("title");
  last_title_ = str ? *str : "";
  str = params.FindString("subtitle");
  last_subtitle_ = str ? std::make_optional(*str) : std::nullopt;
  str = params.FindString("dialogType");
  last_dialog_type_ = str ? *str : "";
  const base::Value::List* accounts = params.FindList("accounts");
  if (accounts) {
    last_accounts_ = accounts->Clone();
  } else {
    last_accounts_ = base::Value::List();
  }
  return Status(kOk);
}
