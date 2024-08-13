// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_FAKE_PASSPOINT_SUBSCRIPTION_H_
#define CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_FAKE_PASSPOINT_SUBSCRIPTION_H_

#include <optional>
#include <string>
#include <vector>

namespace ash::connectivity {

class FakePasspointSubscription {
 public:
  explicit FakePasspointSubscription(const std::string id,
                                     const std::string friendly_name,
                                     const std::string provisioning_source,
                                     std::optional<std::string> trusted_ca,
                                     int64_t expiration_epoch_ms,
                                     const std::vector<std::string> domains);
  ~FakePasspointSubscription();

  FakePasspointSubscription(const FakePasspointSubscription&);
  FakePasspointSubscription& operator=(const FakePasspointSubscription&);

  void AddDomain(const std::string& domain);

  const std::string& id() const { return id_; }
  const std::string& friendly_name() const { return friendly_name_; }
  const std::string& provisioning_source() const {
    return provisioning_source_;
  }
  const std::vector<std::string>& domains() const { return domains_; }
  const std::optional<std::string>& trusted_ca() const { return trusted_ca_; }
  int64_t expiration_epoch_ms() const { return expiration_epoch_ms_; }

 private:
  std::string id_;
  std::string friendly_name_;
  std::string provisioning_source_;
  std::optional<std::string> trusted_ca_;
  int64_t expiration_epoch_ms_;
  std::vector<std::string> domains_;
};

}  // namespace ash::connectivity

#endif  // CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_FAKE_PASSPOINT_SUBSCRIPTION_H_
