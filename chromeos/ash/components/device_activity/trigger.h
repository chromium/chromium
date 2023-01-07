// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_TRIGGER_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_TRIGGER_H_

namespace ash::device_activity {

// Device actives are measured according to trigger enums.
// TODO(https://crbug.com/1262178): Add another trigger for when sign-in occurs.
enum class Trigger {
  kNetwork  // Network state becomes connected.
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_TRIGGER_H_
