// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_details.h"

#include <assert.h>
#include <string.h>

#include <type_traits>

#include "chrome/chrome_elf/nt_registry/nt_registry.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "components/version_info/version_info_values.h"

namespace install_static {

namespace {

// The version is set at compile-time to a W.X.Y.Z value.
constexpr char kProductVersion[] = PRODUCT_VERSION;

// This module's instance (intentionally leaked at shutdown).
const InstallDetails* g_module_details = nullptr;

}  // namespace

std::wstring InstallDetails::GetClientStateKeyPath() const {
  return install_static::GetClientStateKeyPath(app_guid());
}

std::wstring InstallDetails::GetClientStateMediumKeyPath() const {
  return install_static::GetClientStateMediumKeyPath(app_guid());
}

bool InstallDetails::VersionMismatch() const {
  // Check the product version and the size of the mode structure.
  return payload_->size != sizeof(Payload) ||
         strcmp(payload_->product_version, &kProductVersion[0]) != 0 ||
         payload_->mode->size != sizeof(InstallConstants);
}

// static
const InstallDetails& InstallDetails::Get() {
  assert(g_module_details);
  return *g_module_details;
}

// static
void InstallDetails::SetForProcess(
    std::unique_ptr<PrimaryInstallDetails> details) {
  assert(!details || !g_module_details);
  // Tests may set then reset via null. In this case, delete the old instance.
  delete g_module_details;
  // Intentionally leaked at shutdown.
  g_module_details = details.release();
}

// static
const InstallDetails::Payload* InstallDetails::GetPayload() {
  assert(g_module_details);
  static_assert(std::is_pod<Payload>::value, "Payload must be a POD-struct");
  static_assert(std::is_pod<InstallConstants>::value,
                "InstallConstants must be a POD-struct");
  return g_module_details->payload_;
}

// static
void InstallDetails::InitializeFromPayload(
    const InstallDetails::Payload* payload) {
  assert(!g_module_details);
  // Intentionally leaked at shutdown.
  g_module_details = new InstallDetails(payload);
}

// static
std::unique_ptr<const InstallDetails> InstallDetails::Swap(
    std::unique_ptr<const InstallDetails> install_details) {
  std::unique_ptr<const InstallDetails> previous_value(g_module_details);
  g_module_details = install_details.release();
  return previous_value;
}

PrimaryInstallDetails::PrimaryInstallDetails() : InstallDetails(&payload_) {
  payload_.size = sizeof(payload_);
  payload_.product_version = &kProductVersion[0];
}

}  // namespace install_static
