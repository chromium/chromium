// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STYLUS_HANDWRITING_WIN_FEATURES_H_
#define COMPONENTS_STYLUS_HANDWRITING_WIN_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace stylus_handwriting::win {

// Enables stylus writing for Windows platforms.
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
BASE_DECLARE_FEATURE(kStylusHandwritingWin);
COMPONENT_EXPORT(STYLUS_HANDWRITING_WIN)
extern bool IsStylusHandwritingWinEnabled();

}  // namespace stylus_handwriting::win

#endif  // COMPONENTS_STYLUS_HANDWRITING_WIN_FEATURES_H_
