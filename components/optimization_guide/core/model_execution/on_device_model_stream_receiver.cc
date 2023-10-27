// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_stream_receiver.h"

#include "base/strings/strcat.h"

namespace optimization_guide {

OnDeviceModelStreamReceiver::OnDeviceModelStreamReceiver(
    ResultCallback result_callback)
    : result_callback_(std::move(result_callback)) {}

OnDeviceModelStreamReceiver::~OnDeviceModelStreamReceiver() = default;

void OnDeviceModelStreamReceiver::OnResponse(const std::string& response_text) {
  responses_.push_back(response_text);
}

void OnDeviceModelStreamReceiver::OnComplete() {
  std::move(result_callback_).Run(base::StrCat(responses_));
}

}  // namespace optimization_guide
