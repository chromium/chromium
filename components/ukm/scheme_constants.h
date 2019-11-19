// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_SCHEME_CONSTANTS_H_
#define COMPONENTS_UKM_SCHEME_CONSTANTS_H_

namespace ukm {

// Defines several URL scheme constants to avoid dependencies.
// kAppScheme will be defined in code that isn't available here.
extern const char kAppScheme[];
// kChromeUIScheme is defined in content, which this code can't depend on
// since it's used by iOS too.
extern const char kChromeUIScheme[];
// kExtensionScheme is defined in extensions which also isn't available here.
extern const char kExtensionScheme[];

}  // namespace ukm

#endif  // COMPONENTS_UKM_SCHEME_CONSTANTS_H_
