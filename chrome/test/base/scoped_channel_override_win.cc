// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/scoped_channel_override.h"

#include <memory>

#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"

namespace chrome {

namespace {

std::unique_ptr<install_static::PrimaryInstallDetails> MakeDetails(
    ScopedChannelOverride::Channel channel) {
  auto details = std::make_unique<install_static::PrimaryInstallDetails>();

  switch (channel) {
    case ScopedChannelOverride::Channel::kExtendedStable:
      details->set_mode(
          &install_static::kInstallModes[install_static::STABLE_INDEX]);
      details->set_channel_origin(install_static::ChannelOrigin::kPolicy);
      details->set_channel_override(L"extended");
      details->set_is_extended_stable_channel(true);
      break;
    case ScopedChannelOverride::Channel::kStable:
      details->set_mode(
          &install_static::kInstallModes[install_static::STABLE_INDEX]);
      break;
    case ScopedChannelOverride::Channel::kBeta:
      details->set_mode(
          &install_static::kInstallModes[install_static::BETA_INDEX]);
      details->set_channel(L"beta");
      break;
    case ScopedChannelOverride::Channel::kDev:
      details->set_mode(
          &install_static::kInstallModes[install_static::DEV_INDEX]);
      details->set_channel(L"dev");
      break;
    case ScopedChannelOverride::Channel::kCanary:
      details->set_mode(
          &install_static::kInstallModes[install_static::CANARY_INDEX]);
      details->set_channel(L"canary");
      break;
  }

  return details;
}

}  // namespace

ScopedChannelOverride::ScopedChannelOverride(Channel channel)
    : scoped_install_details_(MakeDetails(channel)) {}

ScopedChannelOverride::~ScopedChannelOverride() = default;

}  // namespace chrome
