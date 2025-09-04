// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_

#include "base/memory/weak_ptr.h"

namespace content {
class NavigationHandle;
}

struct NavigateParams;

// Navigates according to the configuration specified in |params|.
// Returns the NavigationHandle* for the started navigation, which might be null
// if the navigation couldn't be started.
base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params);

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
