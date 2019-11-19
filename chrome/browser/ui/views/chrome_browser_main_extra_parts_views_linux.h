// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

// Extra parts, which are used by both Ozone/X11/Wayland and inherited by the
// non-ozone X11 extra parts.
class ChromeBrowserMainExtraPartsViewsLinux
    : public ChromeBrowserMainExtraPartsViews {
 public:
  ChromeBrowserMainExtraPartsViewsLinux();
  ~ChromeBrowserMainExtraPartsViewsLinux() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreEarlyInitialization() override;
  void ToolkitInitialized() override;
  void PreCreateThreads() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsViewsLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LINUX_H_
