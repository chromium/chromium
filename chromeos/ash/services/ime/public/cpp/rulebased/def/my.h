// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_MY_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_MY_H_

namespace my {

// The id of this IME/keyboard.
extern const char* kId;

// Whether this keyboard layout is a 102 or 101 keyboard.
extern bool kIs102;

// The key mapping definitions under various modifier states.
extern const char** kKeyMap[8];

// The transform rules definition. The string items in the even indexes are
// the regular expressions represent what needs to be transformed, and the
// ones in the odd indexes represent it can transform to what.
extern const char* kTransforms[];

// The length of the transform rules.
extern const unsigned int kTransformsLen;

// The history prune regexp.
extern const char* kHistoryPrune;

}  // namespace my

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_MY_H_
