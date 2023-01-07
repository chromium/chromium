// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_TEST_SEND_TAB_TO_SELF_MODEL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_TEST_SEND_TAB_TO_SELF_MODEL_H_

#include <string>
#include <vector>

#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/target_device_info.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;

class TestSendTabToSelfModel : public SendTabToSelfModel {
 public:
  TestSendTabToSelfModel() = default;
  ~TestSendTabToSelfModel() override = default;

  // SendTabToSelfModel:
  std::vector<std::string> GetAllGuids() const override;
  const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override;
  const SendTabToSelfEntry* AddEntry(const GURL& url,
                                     const std::string& title,
                                     const std::string& device_id) override;
  void DeleteEntry(const std::string& guid) override;
  void DismissEntry(const std::string& guid) override;
  void MarkEntryOpened(const std::string& guid) override;

  bool IsReady() override;
  bool HasValidTargetDevice() override;
  std::vector<TargetDeviceInfo> GetTargetDeviceInfoSortedList() override;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_TEST_SEND_TAB_TO_SELF_MODEL_H_
