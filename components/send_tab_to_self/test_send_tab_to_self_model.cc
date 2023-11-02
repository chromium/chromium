// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/test_send_tab_to_self_model.h"

namespace send_tab_to_self {

std::vector<std::string> TestSendTabToSelfModel::GetAllGuids() const {
  return {};
}

const SendTabToSelfEntry* TestSendTabToSelfModel::GetEntryByGUID(
    const std::string& guid) const {
  return nullptr;
}

const SendTabToSelfEntry* TestSendTabToSelfModel::AddEntry(
    const GURL& url,
    const std::string& title,
    const std::string& device_id) {
  return nullptr;
}

void TestSendTabToSelfModel::DeleteEntry(const std::string& guid) {}

void TestSendTabToSelfModel::DismissEntry(const std::string& guid) {}

void TestSendTabToSelfModel::MarkEntryOpened(const std::string& guid) {}

bool TestSendTabToSelfModel::IsReady() {
  return false;
}

bool TestSendTabToSelfModel::HasValidTargetDevice() {
  return false;
}

std::vector<TargetDeviceInfo>
TestSendTabToSelfModel::GetTargetDeviceInfoSortedList() {
  return {};
}

}  // namespace send_tab_to_self
