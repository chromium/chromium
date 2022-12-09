// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/cast_tracker.h"

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

CastTracker::CastTracker(DevToolsClient* client)
    : sinks_(base::Value::List()), issue_("") {
  client->AddListener(this);
  client->SendCommand("Cast.enable", base::Value::Dict());
}

CastTracker::~CastTracker() = default;

bool CastTracker::ListensToConnections() const {
  return false;
}

Status CastTracker::OnEvent(DevToolsClient* client,
                            const std::string& method,
                            const base::Value::Dict& params) {
  if (method == "Cast.sinksUpdated") {
    const base::Value* sinks = params.Find("sinks");
    if (sinks)
      sinks_ = sinks->Clone();
  } else if (method == "Cast.issueUpdated") {
    const base::Value* issue = params.Find("issueMessage");
    if (issue)
      issue_ = issue->Clone();
  }
  return Status(kOk);
}
