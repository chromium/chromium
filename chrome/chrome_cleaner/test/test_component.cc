// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_component.h"

namespace chrome_cleaner {

TestComponent::Calls::Calls() = default;
TestComponent::Calls::~Calls() = default;

TestComponent::~TestComponent() {
  calls_->destroyed = true;
}

void TestComponent::PreScan() {
  calls_->pre_scan = true;
}

void TestComponent::PostScan(const std::vector<UwSId>& found_pups) {
  calls_->post_scan = true;
  calls_->post_scan_found_pups = found_pups;
}

void TestComponent::PreCleanup() {
  calls_->pre_cleanup = true;
}

void TestComponent::PostCleanup(ResultCode result_code, RebooterAPI* rebooter) {
  calls_->result_code = result_code;
  calls_->post_cleanup = true;
}

void TestComponent::PostValidation(ResultCode result_code) {
  calls_->result_code = result_code;
  calls_->post_validation = true;
}

void TestComponent::OnClose(ResultCode result_code) {
  calls_->result_code = result_code;
  calls_->on_close = true;
}

}  // namespace chrome_cleaner
