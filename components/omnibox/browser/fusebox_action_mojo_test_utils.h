// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FUSEBOX_ACTION_MOJO_TEST_UTILS_H_
#define COMPONENTS_OMNIBOX_BROWSER_FUSEBOX_ACTION_MOJO_TEST_UTILS_H_

#include <iosfwd>

#include "components/omnibox/browser/fusebox_action.mojom.h"

namespace fusebox_action::mojom {

// Debug-printing functions for FuseboxAction mojo types.
void PrintTo(const FuseboxAction& action, std::ostream* os);
void PrintTo(const FuseboxActionPtr& action, std::ostream* os);

}  // namespace fusebox_action::mojom

#endif  // COMPONENTS_OMNIBOX_BROWSER_FUSEBOX_ACTION_MOJO_TEST_UTILS_H_
