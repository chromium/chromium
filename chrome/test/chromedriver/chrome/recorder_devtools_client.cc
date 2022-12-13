// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"

#include <memory>

#include "chrome/test/chromedriver/chrome/status.h"

RecorderDevToolsClient::RecorderDevToolsClient() {}

RecorderDevToolsClient::~RecorderDevToolsClient() {}

Status RecorderDevToolsClient::SendCommandAndGetResult(
    const std::string& method,
    const base::Value::Dict& params,
    base::Value::Dict* result) {
  commands_.emplace_back(method, params.Clone());

  // For any tests that directly call SendCommandAndGetResults, we'll just
  // always return { "result": true }. Currently only used when testing
  // "canEmulateNetworkConditions".
  result->Set("result", true);
  return Status(kOk);
}
