// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace ash {

StubInstallAttributes::StubInstallAttributes() : InstallAttributes(nullptr) {
  device_locked_ = true;
}

// static
std::unique_ptr<StubInstallAttributes> StubInstallAttributes::CreateUnset() {
  auto result = std::make_unique<StubInstallAttributes>();
  result->Clear();
  return result;
}

// static
std::unique_ptr<StubInstallAttributes>
StubInstallAttributes::CreateConsumerOwned() {
  auto result = std::make_unique<StubInstallAttributes>();
  result->SetConsumerOwned();
  return result;
}

// static
std::unique_ptr<StubInstallAttributes>
StubInstallAttributes::CreateCloudManaged(const std::string& domain,
                                          const std::string& device_id) {
  auto result = std::make_unique<StubInstallAttributes>();
  result->SetCloudManaged(domain, device_id);
  return result;
}

// static
std::unique_ptr<StubInstallAttributes> StubInstallAttributes::CreateDemoMode() {
  auto result = std::make_unique<StubInstallAttributes>();
  result->SetDemoMode();
  return result;
}

void StubInstallAttributes::Clear() {
  registration_mode_ = policy::DEVICE_MODE_NOT_SET;
  registration_domain_.clear();
  registration_realm_.clear();
  registration_device_id_.clear();
}

void StubInstallAttributes::SetConsumerOwned() {
  registration_mode_ = policy::DEVICE_MODE_CONSUMER;
  registration_domain_.clear();
  registration_realm_.clear();
  registration_device_id_.clear();
}

void StubInstallAttributes::SetCloudManaged(const std::string& domain,
                                            const std::string& device_id) {
  registration_mode_ = policy::DEVICE_MODE_ENTERPRISE;
  registration_domain_ = domain;
  registration_realm_.clear();
  registration_device_id_ = device_id;
}

void StubInstallAttributes::SetDemoMode() {
  registration_mode_ = policy::DEVICE_MODE_DEMO;
  registration_domain_ = policy::kDemoModeDomain;
  registration_realm_.clear();
  registration_device_id_ = "demo-device-id";
}

ScopedStubInstallAttributes::ScopedStubInstallAttributes()
    : ScopedStubInstallAttributes(std::make_unique<StubInstallAttributes>()) {}

ScopedStubInstallAttributes::ScopedStubInstallAttributes(
    std::unique_ptr<StubInstallAttributes> install_attributes)
    : install_attributes_(std::move(install_attributes)) {
  // The constructor calls SetForTesting with these install_attributes, so
  // in the destructor we make the matching call to ShutdownForTesting.
  InstallAttributes::SetForTesting(install_attributes_.get());
}

ScopedStubInstallAttributes::~ScopedStubInstallAttributes() {
  // Make sure that the install_attributes_ that are held by this class
  // are still the same as the global singleton - if not, then that singleton
  // has been overwritten somewhere else, which is probably a bug.
  CHECK_EQ(install_attributes_.get(), InstallAttributes::Get());

  InstallAttributes::ShutdownForTesting();  // Unset the global singleton.
}

StubInstallAttributes* ScopedStubInstallAttributes::Get() {
  return install_attributes_.get();
}

}  // namespace ash
