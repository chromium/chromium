// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_UTILS_H_

#import "base/values.h"

namespace dom_distiller {

// Returns the extracted result for `OnDistillationDone` from the DOM distiller
// Javascript result.
base::Value ParseValueFromScriptResult(const base::Value* value);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_UTILS_H_
