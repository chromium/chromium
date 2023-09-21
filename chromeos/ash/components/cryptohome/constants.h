// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CONSTANTS_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CONSTANTS_H_

#include "base/time/time.h"

namespace cryptohome {

// By default Authsession is valid for 300 seconds after becoming
// authenticated.
constexpr base::TimeDelta kAuthsessionInitialLifetime = base::Minutes(5);

// When extending Authsession lifetime, we ask to extend it by 60 seconds.
constexpr base::TimeDelta kAuthsessionExtensionPeriod = base::Minutes(1);

// Chrome would request Authsession lifetime extension when there is
// less than 15 seconds remaining.
constexpr base::TimeDelta kAuthsessionExtendThreshold = base::Seconds(15);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_CONSTANTS_H_
