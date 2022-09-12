// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CRYPTOHOME_ACCOUNT_IDENTIFIER_OPERATORS_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CRYPTOHOME_ACCOUNT_IDENTIFIER_OPERATORS_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"

namespace cryptohome {

// operator< to use AccountIdentifier in STL containers.
COMPONENT_EXPORT(CRYPTOHOME_CLIENT)
bool operator<(const AccountIdentifier& l, const AccountIdentifier& r);

// operator== to use AccountIdentifier in tests.
COMPONENT_EXPORT(CRYPTOHOME_CLIENT)
bool operator==(const AccountIdentifier& l, const AccountIdentifier& r);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CRYPTOHOME_ACCOUNT_IDENTIFIER_OPERATORS_H_
