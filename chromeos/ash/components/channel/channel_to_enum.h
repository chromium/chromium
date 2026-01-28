// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHANNEL_CHANNEL_TO_ENUM_H_
#define CHROMEOS_ASH_COMPONENTS_CHANNEL_CHANNEL_TO_ENUM_H_

#include <string_view>

#include "base/component_export.h"
#include "components/version_info/channel.h"

namespace ash {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CHANNEL)
version_info::Channel ChannelToEnum(std::string_view channel);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_CHANNEL_CHANNEL_TO_ENUM_H_
