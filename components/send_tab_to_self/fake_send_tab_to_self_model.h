// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FAKE_SEND_TAB_TO_SELF_MODEL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FAKE_SEND_TAB_TO_SELF_MODEL_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
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
      std::string_view guid) const override;
  std::vector<const SendTabToSelfEntry*>
  GetUnopenedEntriesTargetedToLocalDevice() const override;
  std::vector<const SendTabToSelfEntry*> GetOpenedEntriesTargetedToLocalDevice()
      const override;
  const SendTabToSelfEntry* SendEntry(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid,
      const PageContext& context,
      NavigationHistory navigation_history,
      base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation,
      ShareEntryPoint entry_point) override;
  void DismissEntry(std::string_view guid) override;
  void MarkEntryOpened(std::string_view guid) override;
  void MarkEntryActivated(std::string_view guid,
                          ShareActivatedEntryPoint entry_point) override;
  bool IsReady() override;
  bool HasValidTargetDevice() override;
  std::vector<TargetDeviceInfo> GetTargetDeviceInfoSortedList() override;
  std::optional<TargetDeviceInfo> GetTargetDeviceInfo(
      std::string_view cache_guid) override;

  // Methods to configure the fake behavior:
  void SetIsReady(bool is_ready);
  void SetHasValidTargetDevice(bool has_valid_target_device);
  void SetTargetDeviceInfoSortedList(
      const std::vector<TargetDeviceInfo>& devices);
  void AddTargetDevice(const TargetDeviceInfo& device);
  void SetLocalDeviceName(std::string_view device_name);
  void SetLocalCacheGuid(std::string_view cache_guid);
  void SetSendResult(SendTabToSelfResult result);

  using SendEntryCallback =
      base::RepeatingCallback<void(const SendTabToSelfEntry*)>;
  void SetSendEntryCallback(SendEntryCallback callback);

  struct RemoteEntryParams {
    GURL url;
    std::string title;
    std::string target_device_cache_guid;
    PageContext context = PageContext();
    NavigationHistory navigation_history = {};
    base::Time shared_time = base::Time();
  };

  // Simulates an entry being added from a remote device.
  const SendTabToSelfEntry* AddEntryRemotely(RemoteEntryParams params);

  const SendTabToSelfEntry* AddEntryRemotely(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid,
      const PageContext& context,
      NavigationHistory navigation_history);

  // Simulates multiple entries being added from a remote device in a single
  // batch.
  std::vector<const SendTabToSelfEntry*> AddEntriesRemotely(
      std::vector<RemoteEntryParams> entries_params);

  // Simulates an entry being removed from a remote device.
  void RemoveEntryRemotely(const std::string& guid);

  const std::string& last_opened_guid() const { return last_opened_guid_; }
  const std::string& last_dismissed_guid() const {
    return last_dismissed_guid_;
  }
  const std::string& last_activated_guid() const {
    return last_activated_guid_;
  }
  std::optional<ShareActivatedEntryPoint> last_activated_entry_point() const {
    return last_activated_entry_point_;
  }
  int activated_call_count() const { return activated_call_count_; }

 private:
  bool is_ready_ = true;
  bool has_valid_target_device_ = false;
  std::string local_device_name_ = "device";
  std::string local_cache_guid_ = "";
  std::map<std::string, std::unique_ptr<SendTabToSelfEntry>, std::less<>>
      entries_;
  std::vector<TargetDeviceInfo> devices_;
  std::string last_opened_guid_;
  std::string last_dismissed_guid_;
  std::string last_activated_guid_;
  std::optional<ShareActivatedEntryPoint> last_activated_entry_point_;
  int activated_call_count_ = 0;
  SendEntryCallback send_entry_callback_;
  SendTabToSelfResult send_result_ = SendTabToSelfResult::kSuccess;
  std::vector<std::unique_ptr<SendTabToSelfEntry>>
      remote_entries_pending_model_ready_;
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FAKE_SEND_TAB_TO_SELF_MODEL_H_
