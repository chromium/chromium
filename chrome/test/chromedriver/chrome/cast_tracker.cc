// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/cast_tracker.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

CastTracker::CastTracker(DevToolsClient* client)
    : sinks_(std::vector<base::Value>()), issue_("") {
  client->ConnectIfNecessary();
  client->AddListener(this);
  client->SendCommand("Cast.enable", base::DictionaryValue());
}

CastTracker::~CastTracker() = default;

Status CastTracker::OnEvent(DevToolsClient* client,
                            const std::string& method,
                            const base::DictionaryValue& params) {
  if (method == "Cast.sinksUpdated") {
    const base::Value* sinks = params.FindKey("sinks");
    if (sinks)
      sinks_ = sinks->Clone();
  } else if (method == "Cast.issueUpdated") {
    const base::Value* issue = params.FindKey("issueMessage");
    if (issue)
      issue_ = issue->Clone();
  }
  return Status(kOk);
}
