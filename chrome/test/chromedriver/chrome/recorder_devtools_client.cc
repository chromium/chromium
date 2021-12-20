// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"

#include <memory>

#include "chrome/test/chromedriver/chrome/status.h"

RecorderDevToolsClient::RecorderDevToolsClient() {}

RecorderDevToolsClient::~RecorderDevToolsClient() {}

Status RecorderDevToolsClient::SendCommandAndGetResult(
    const std::string& method,
    const base::DictionaryValue& params,
    base::Value* result) {
  commands_.emplace_back(method, params);

  // For any tests that directly call SendCommandAndGetResults, we'll just
  // always return { "result": true }. Currently only used when testing
  // "canEmulateNetworkConditions".
  *result = base::Value(base::Value::Type::DICTIONARY);
  result->SetBoolKey("result", true);
  return Status(kOk);
}
