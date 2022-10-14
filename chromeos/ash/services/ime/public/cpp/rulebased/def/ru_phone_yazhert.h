// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_RU_PHONE_YAZHERT_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_RU_PHONE_YAZHERT_H_

namespace ru_phone_yazhert {

// The id of this IME/keyboard.
extern const char* kId;

// Whether this keyboard layout is a 102 or 101 keyboard.
extern bool kIs102;

// The key mapping definitions under various modifier states.
extern const char** kKeyMap[8];

}  // namespace ru_phone_yazhert

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_RU_PHONE_YAZHERT_H_
