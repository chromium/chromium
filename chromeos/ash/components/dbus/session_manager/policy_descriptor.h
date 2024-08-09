// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_POLICY_DESCRIPTOR_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_POLICY_DESCRIPTOR_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/login_manager/policy_descriptor.pb.h"

namespace ash {

// Creates a PolicyDescriptor object to store/retrieve Chrome policy.
COMPONENT_EXPORT(SESSION_MANAGER)
login_manager::PolicyDescriptor MakeChromePolicyDescriptor(
    login_manager::PolicyAccountType account_type,
    const std::string& account_id);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SESSION_MANAGER_POLICY_DESCRIPTOR_H_
