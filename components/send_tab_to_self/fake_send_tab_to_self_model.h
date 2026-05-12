// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FAKE_SEND_TAB_TO_SELF_MODEL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FAKE_SEND_TAB_TO_SELF_MODEL_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

class GURL;

namespace send_tab_to_self {

class SendTabToSelfEntry;
struct NavigationHistory;
struct PageContext;
struct TargetDeviceInfo;

class FakeSendTabToSelfModel final : public SendTabToSelfModel {
 public:
  FakeSendTabToSelfModel();
  ~FakeSendTabToSelfModel() override;

  // SendTabToSelfModel:
  std::vector<std::string> GetAllGuids() const override;
  const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override;
  const SendTabToSelfEntry* SendEntry(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid,
      const PageContext& context,
      NavigationHistory navigation_history,
      base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation)
      override;
  void DismissEntry(const std::string& guid) override;
  void MarkEntryOpened(const std::string& guid) override;
  bool IsReady() override;
  bool HasValidTargetDevice() override;
  std::vector<TargetDeviceInfo> GetTargetDeviceInfoSortedList() override;

  // Methods to configure the fake behavior:
  void SetIsReady(bool is_ready);
  void SetHasValidTargetDevice(bool has_valid_target_device);
  void SetTargetDeviceInfoSortedList(
      const std::vector<TargetDeviceInfo>& devices);
  void AddTargetDevice(const TargetDeviceInfo& device);
  void SetLocalDeviceName(std::string_view device_name);
  void SetSendResult(SendTabToSelfResult result);

  using SendEntryCallback =
      base::RepeatingCallback<void(const SendTabToSelfEntry*)>;
  void SetSendEntryCallback(SendEntryCallback callback);

  // Simulates an entry being added from a remote device.
  const SendTabToSelfEntry* AddEntryRemotely(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid,
      const PageContext& context,
      NavigationHistory navigation_history);

  const std::string& last_opened_guid() const { return last_opened_guid_; }
  const std::string& last_dismissed_guid() const {
    return last_dismissed_guid_;
  }

 private:
  bool is_ready_ = true;
  bool has_valid_target_device_ = false;
  std::string local_device_name_ = "device";
  std::map<std::string, std::unique_ptr<SendTabToSelfEntry>> entries_;
  std::vector<TargetDeviceInfo> devices_;
  std::string last_opened_guid_;
  std::string last_dismissed_guid_;
  SendEntryCallback send_entry_callback_;
  SendTabToSelfResult send_result_ = SendTabToSelfResult::kSuccess;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FAKE_SEND_TAB_TO_SELF_MODEL_H_
