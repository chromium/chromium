// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_COMMON_TYPES_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_COMMON_TYPES_H_

#include <string>

#include "base/types/strong_alias.h"

namespace cryptohome {

// The label that uniquely identifies a key among all keys configured for a
// user.
// We use a strong alias to avoid accidentally mixing up key labels with other
// variables of type `std::string`.
using KeyLabel = base::StrongAlias<class KeyLabelTag, std::string>;

// The PIN as the user would enter it. Not salted or hashed.
using RawPin = base::StrongAlias<class RawPinTag, std::string>;

// The salt we use for PINs.
using PinSalt = base::StrongAlias<class PinSaltTag, std::string>;

// The password as the user would enter it. Not salted or hashed.
using RawPassword = base::StrongAlias<class RawPasswordTag, std::string>;

// Type that denotes version of software component (Chrome or ChromeOS)
// that was used to set up a factor.
using ComponentVersion =
    base::StrongAlias<class ComponentVersionTag, std::string>;

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_COMMON_TYPES_H_
