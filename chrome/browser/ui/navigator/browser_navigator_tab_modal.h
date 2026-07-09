// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NAVIGATOR_BROWSER_NAVIGATOR_TAB_MODAL_H_
#define CHROME_BROWSER_UI_NAVIGATOR_BROWSER_NAVIGATOR_TAB_MODAL_H_

#include <concepts>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace gfx {
class Size;
}

namespace autofill::payments {
class DesktopPaymentsWindowManager;
}

// Provides functions for showing tab-modal popup browsers.
class BrowserNavigatorTabModal {
 public:
  BrowserNavigatorTabModal() = delete;

  // Show `url` as a modal popup for the tab of `source_contents`, with `size`.
  //
  // Only certain systems are allowed to do this. If you need to add to the list
  // of allowed classes, be sure to check with code OWNERS.
  template <typename T>
    requires std::same_as<T, autofill::payments::DesktopPaymentsWindowManager>
  static base::WeakPtr<content::NavigationHandle> Show(
      const GURL& url,
      content::WebContents& source_contents,
      const gfx::Size& size,
      base::PassKey<T>) {
    return ShowImpl(url, source_contents, size);
  }

  inline static base::WeakPtr<content::NavigationHandle> ShowForTesting(
      const GURL& url,
      content::WebContents& source_contents,
      const gfx::Size& size) {
    return ShowImpl(url, source_contents, size);
  }

 private:
  static base::WeakPtr<content::NavigationHandle> ShowImpl(
      const GURL& url,
      content::WebContents& source_contents,
      const gfx::Size& size);
};

#endif  // CHROME_BROWSER_UI_NAVIGATOR_BROWSER_NAVIGATOR_TAB_MODAL_H_
