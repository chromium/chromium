// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_US_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_US_H_

namespace us {

// The key mapping definitions under various modifier states for the US layout.
extern const char** kKeyMap[8];

// The Id for the US keyboard.
extern const char* kId;

// Whether the US keyboard is 102 or 101 keyboard.
extern const bool kIs102;

}  // namespace us

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_US_H_
