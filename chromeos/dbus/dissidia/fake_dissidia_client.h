// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DISSIDIA_FAKE_DISSIDIA_CLIENT_H_
#define CHROMEOS_DBUS_DISSIDIA_FAKE_DISSIDIA_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dissidia/dissidia_client.h"

namespace chromeos {

class COMPONENT_EXPORT(DISSIDIA) FakeDissidiaClient : public DissidiaClient {
 public:
  FakeDissidiaClient();
  FakeDissidiaClient(const FakeDissidiaClient&) = delete;
  FakeDissidiaClient& operator=(const FakeDissidiaClient&) = delete;
  ~FakeDissidiaClient() override;

  // DissidiaClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void PerformUpdate(const std::string& target,
                     PerformUpdateCallback callback) override;

  void set_update_status(dissidia::PerformUpdateStatus status) {
    update_status_ = status;
  }

  void set_update_message(const std::string& message) {
    update_message_ = message;
  }

  void NotifyProgress(int32_t percent, const std::string& stage);

  void NotifyCompleted(bool success,
                       dissidia::CompletedErrorCode error_code,
                       const std::string& message);

  const std::string& last_target() const { return last_target_; }

  int perform_update_call_count() const { return perform_update_call_count_; }

 private:
  dissidia::PerformUpdateStatus update_status_ = dissidia::kUpdateStarted;
  std::string update_message_ = "Update started successfully.";
  std::string last_target_;
  int perform_update_call_count_ = 0;

  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DISSIDIA_FAKE_DISSIDIA_CLIENT_H_
