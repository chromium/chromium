// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"

namespace content {
class NavigationHandle;
}

struct NavigateParams;

// Navigates according to the configuration specified in |params|.
// Returns the NavigationHandle* for the started navigation, which might be null
// if the navigation couldn't be started.
// Note: Prefer asynchronous version for Android code. On Android, if
// params->disposition would create a new window, this will return a nullptr
// with no navigation.
base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params);

// Follows the provided |params|. NavigationHandle* return value is provided to
// the OnceCallback.
// Note: Always recommended for Android navigations.
void Navigate(NavigateParams* params,
              base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)>
                  callback);

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
