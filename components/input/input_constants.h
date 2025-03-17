// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_CONSTANTS_H_
#define COMPONENTS_INPUT_INPUT_CONSTANTS_H_

#include "base/time/time.h"

namespace input {

#if BUILDFLAG(IS_ANDROID)
// The mobile hang timer is shorter than the desktop hang timer because the
// screen is smaller and more intimate, and therefore requires more nimbleness.
inline constexpr base::TimeDelta kHungRendererDelay = base::Seconds(5);
#else
// It would be nice to lower the desktop delay, but going any further with the
// modal dialog UI would be disruptive, and while new gentle UI indicating that
// a page is hung would be great, that UI isn't going to happen any time soon.
inline constexpr base::TimeDelta kHungRendererDelay = base::Seconds(15);
#endif

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_CONSTANTS_H_
