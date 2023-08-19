// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TRIBOOL_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TRIBOOL_H_

#include <string>

namespace signin {

// The values are persisted to disk and must not be changed.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin
enum class Tribool { kUnknown = -1, kFalse = 0, kTrue = 1 };

// Returns `kTrue` if `b` is true and `kFalse` otherwise.
// Guaranteed to never return `kUnknown`.
Tribool TriboolFromBool(bool b);

// Returns the bool value of `tribool` and crashes if `tribool` is `kUnknown`.
bool TriboolToBoolOrDie(Tribool tribool);

// Returns the string representation of a tribool.
std::string TriboolToString(Tribool tribool);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TRIBOOL_H_
