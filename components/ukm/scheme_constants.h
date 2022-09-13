// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_SCHEME_CONSTANTS_H_
#define COMPONENTS_UKM_SCHEME_CONSTANTS_H_

#include "base/component_export.h"

namespace ukm {

// Defines several URL scheme constants to avoid dependencies.
// kAppScheme will be defined in code that isn't available here.
COMPONENT_EXPORT(UKM_RECORDER)
extern const char kAppScheme[];
// kChromeUIScheme is defined in content, which this code can't depend on
// since it's used by iOS too.
COMPONENT_EXPORT(UKM_RECORDER)
extern const char kChromeUIScheme[];
// kExtensionScheme is defined in extensions which also isn't available here.
COMPONENT_EXPORT(UKM_RECORDER)
extern const char kExtensionScheme[];

}  // namespace ukm

#endif  // COMPONENTS_UKM_SCHEME_CONSTANTS_H_
