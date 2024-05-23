// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHANNEL_CHANNEL_INFO_H_
#define CHROMEOS_ASH_COMPONENTS_CHANNEL_CHANNEL_INFO_H_

#include <string>

#include "base/component_export.h"

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash {

// Returns the name of the Ash's channel. For a branded build, this modifier is
// the channel ("cannary", "dev", or "beta", but "" for stable and "unknown"
// when we cannot determine the channel). For a non-branded build, always return
// "" as a name.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CHANNEL)
std::string GetChannelName();

// Returns the Ash's channel. For a non-branded build or a branded build when
// the channel cannot be determined, it returns version_info::Channel::Unknown.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CHANNEL)
version_info::Channel GetChannel();

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_CHANNEL_CHANNEL_INFO_H_
