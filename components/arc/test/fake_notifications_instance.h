// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_

#include <string>
#include <utility>
#include <vector>

#include "components/arc/mojom/notifications.mojom.h"

namespace arc {

class FakeNotificationsInstance : public mojom::NotificationsInstance {
 public:
  FakeNotificationsInstance();
  ~FakeNotificationsInstance() override;

  // mojom::NotificationsInstance overrides:
  void InitDeprecated(mojom::NotificationsHostPtr host_ptr) override;
  void Init(mojom::NotificationsHostPtr host_ptr,
            InitCallback callback) override;

  void SendNotificationEventToAndroid(
      const std::string& key,
      mojom::ArcNotificationEvent event) override;
  void CreateNotificationWindow(const std::string& key) override;
  void CloseNotificationWindow(const std::string& key) override;
  void OpenNotificationSettings(const std::string& key) override;
  void OpenNotificationSnoozeSettings(const std::string& key) override;
  void SetDoNotDisturbStatusOnAndroid(
      mojom::ArcDoNotDisturbStatusPtr status) override;
  void CancelPress(const std::string& key) override;
  void PerformDeferredUserAction(uint32_t action_id) override;
  void CancelDeferredUserAction(uint32_t action_id) override;
  void SetLockScreenSettingOnAndroid(
      mojom::ArcLockScreenNotificationSettingPtr setting) override;
  void SetNotificationConfiguration(
      mojom::NotificationConfigurationPtr configuration) override;

  const std::vector<std::pair<std::string, mojom::ArcNotificationEvent>>&
  events() const;
  const mojom::ArcDoNotDisturbStatusPtr& latest_do_not_disturb_status() const;

 private:
  std::vector<std::pair<std::string, mojom::ArcNotificationEvent>> events_;
  mojom::ArcDoNotDisturbStatusPtr latest_do_not_disturb_status_;

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationsInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
