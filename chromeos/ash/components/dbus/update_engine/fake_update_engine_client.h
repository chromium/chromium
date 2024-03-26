// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_UPDATE_ENGINE_FAKE_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_UPDATE_ENGINE_FAKE_UPDATE_ENGINE_CLIENT_H_

#include <map>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

namespace ash {

// A fake implementation of UpdateEngineClient. The user of this class can
// use set_update_engine_client_status() to set a fake last Status and
// GetLastStatus() returns the fake with no modification. Other methods do
// nothing.
class COMPONENT_EXPORT(ASH_DBUS_UPDATE_ENGINE) FakeUpdateEngineClient
    : public UpdateEngineClient {
 public:
  FakeUpdateEngineClient();

  FakeUpdateEngineClient(const FakeUpdateEngineClient&) = delete;
  FakeUpdateEngineClient& operator=(const FakeUpdateEngineClient&) = delete;

  ~FakeUpdateEngineClient() override;

  // UpdateEngineClient overrides
  void Init(dbus::Bus* bus) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  void RequestUpdateCheck(UpdateCheckCallback callback) override;
  void RequestUpdateCheckWithoutApplying(UpdateCheckCallback callback) override;
  void RebootAfterUpdate() override;
  void Rollback() override;
  void CanRollbackCheck(RollbackCheckCallback callback) override;
  update_engine::StatusResult GetLastStatus() override;
  void SetChannel(const std::string& target_channel,
                  bool is_powerwash_allowed) override;
  void GetChannel(bool get_current_channel,
                  GetChannelCallback callback) override;
  void GetEolInfo(GetEolInfoCallback callback) override;
  void SetUpdateOverCellularPermission(bool allowed,
                                       base::OnceClosure callback) override;
  void SetUpdateOverCellularOneTimePermission(
      const std::string& target_version,
      int64_t target_size,
      UpdateOverCellularOneTimePermissionCallback callback) override;
  void ToggleFeature(const std::string& feature, bool enable) override;
  void IsFeatureEnabled(const std::string& feature,
                        IsFeatureEnabledCallback callback) override;
  void ApplyDeferredUpdate(bool shutdown_after_update,
                           base::OnceClosure failure_callback) override;
  // Pushes update_engine::StatusResult in the queue to test changing status.
  // GetLastStatus() returns the status set by this method in FIFO order.
  // See set_default_status().
  void PushLastStatus(const update_engine::StatusResult& status) {
    status_queue_.push(status);
  }

  // Sends status change notification.
  void NotifyObserversThatStatusChanged(
      const update_engine::StatusResult& status);

  // Notifies observers that the user's one time permission is granted.
  void NotifyUpdateOverCellularOneTimePermissionGranted();

  // Sets the default update_engine::StatusResult. GetLastStatus() returns the
  // value set here if |status_queue_| is empty.
  void set_default_status(const update_engine::StatusResult& status);

  // Sets the whole EolInfo to be used when checking eol info.
  void set_eol_info(const EolInfo& eol_info) { eol_info_ = eol_info; }

  // Sets the eol date to be used when checking eol info.
  void set_eol_date(const base::Time& eol_date) {
    eol_info_.eol_date = eol_date;
  }

  // Sets a value returned by RequestUpdateCheck().
  void set_update_check_result(
      const UpdateEngineClient::UpdateCheckResult& result);

  void set_reboot_after_update_callback(base::OnceClosure callback) {
    reboot_after_update_callback_ = std::move(callback);
  }

  void set_can_rollback_check_result(bool result) {
    can_rollback_stub_result_ = result;
  }

  // Returns how many times RebootAfterUpdate() is called.
  int reboot_after_update_call_count() const {
    return reboot_after_update_call_count_;
  }

  // Returns how many times RequestUpdateCheck() is called.
  int request_update_check_call_count() const {
    return request_update_check_call_count_;
  }

  // Returns how many times RequestUpdateCheckWithoutApplying() is called.
  int request_update_check_skip_applying_call_count() const {
    return request_update_check_without_applying_call_count_;
  }

  // Returns how many times Rollback() is called.
  int rollback_call_count() const { return rollback_call_count_; }

  // Returns how many times CanRollback() is called.
  int can_rollback_call_count() const { return can_rollback_call_count_; }

  // Returns how many times |SetUpdateOverCellularPermission()| is called.
  int update_over_cellular_permission_count() const {
    return update_over_cellular_permission_count_;
  }

  // Returns how many times |SetUpdateOverCellularOneTimePermission()| is
  // called.
  int update_over_cellular_one_time_permission_count() const {
    return update_over_cellular_one_time_permission_count_;
  }

  // Returns how many times |ToggleFeature()| is called.
  int toggle_feature_count() const { return toggle_feature_count_; }

  // Returns how many times |IsFeatureEnabled()| is called.
  int is_feature_enabled_count() const { return is_feature_enabled_count_; }

  // Returns how many times |ApplyDeferredUpdate()| is called.
  int apply_deferred_update_count() const {
    return apply_deferred_update_count_;
  }

  void SetToggleFeature(const std::string& feature,
                        std::optional<bool> opt_enabled);

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  base::queue<update_engine::StatusResult> status_queue_;
  update_engine::StatusResult default_status_;
  UpdateCheckResult update_check_result_ = UPDATE_RESULT_SUCCESS;
  base::OnceClosure reboot_after_update_callback_;
  bool can_rollback_stub_result_ = false;
  int reboot_after_update_call_count_ = 0;
  int request_update_check_call_count_ = 0;
  int request_update_check_without_applying_call_count_ = 0;
  int rollback_call_count_ = 0;
  int can_rollback_call_count_ = 0;
  int update_over_cellular_permission_count_ = 0;
  int update_over_cellular_one_time_permission_count_ = 0;
  int toggle_feature_count_ = 0;
  int is_feature_enabled_count_ = 0;
  int apply_deferred_update_count_ = 0;
  std::map<std::string, std::optional<bool>> features_;
  EolInfo eol_info_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_UPDATE_ENGINE_FAKE_UPDATE_ENGINE_CLIENT_H_
