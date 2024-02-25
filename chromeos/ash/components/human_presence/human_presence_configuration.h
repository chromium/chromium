// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HUMAN_PRESENCE_HUMAN_PRESENCE_CONFIGURATION_H_
#define CHROMEOS_ASH_COMPONENTS_HUMAN_PRESENCE_HUMAN_PRESENCE_CONFIGURATION_H_

#include <optional>

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace hps {

// Gets FeatureConfig for enabling LockOnLeave from Finch.
// Returns nullopt if feature is not enabled or can't be parsed correctly.
COMPONENT_EXPORT(HPS)
std::optional<hps::FeatureConfig> GetEnableLockOnLeaveConfig();

// Gets FeatureConfig for enabling SnoopingProtection from Finch.
// Returns nullopt if feature is not enabled or can't be parsed correctly.
COMPONENT_EXPORT(HPS)
std::optional<hps::FeatureConfig> GetEnableSnoopingProtectionConfig();

// Gets quick dim delay to configure power_manager.
COMPONENT_EXPORT(HPS) base::TimeDelta GetQuickDimDelay();

// Gets quick lock delay to configure power_manager.
COMPONENT_EXPORT(HPS) base::TimeDelta GetQuickLockDelay();

// If true, quick dim functionality should be temporarily disabled when a quick
// dim is undimmed within a short period of time.
// Used to configure power_manager.
COMPONENT_EXPORT(HPS) bool GetQuickDimFeedbackEnabled();

// Gets the window following a positive signal in which snooping protection
// should continue to report snooper presence.
COMPONENT_EXPORT(HPS) base::TimeDelta GetSnoopingProtectionPositiveWindow();

}  // namespace hps

#endif  // CHROMEOS_ASH_COMPONENTS_HUMAN_PRESENCE_HUMAN_PRESENCE_CONFIGURATION_H_
