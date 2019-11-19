// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_FEATURES_H_
#define COMPONENTS_PRINTING_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace printing {
namespace features {

#if defined(OS_MACOSX)
extern const base::Feature kEnableCustomMacPaperSizes;
#endif

}  // namespace features
}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_FEATURES_H_
