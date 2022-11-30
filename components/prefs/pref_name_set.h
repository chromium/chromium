// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_NAME_SET_H_
#define COMPONENTS_PREFS_PREF_NAME_SET_H_

#include <functional>
#include <set>
#include <string>

// String set that allows transparent lookup by string-comparable types like
// `base::StringPiece` without requiring conversion to `std::string`.
using PrefNameSet = std::set<std::string, std::less<>>;

#endif  // COMPONENTS_PREFS_PREF_NAME_SET_H_
