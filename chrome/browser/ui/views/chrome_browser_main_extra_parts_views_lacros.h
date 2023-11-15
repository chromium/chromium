// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LACROS_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LACROS_H_

#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"

#include <memory>

class FloatControllerLacros;
class ImmersiveContextLacros;
class SnapControllerLacros;

class ChromeBrowserMainExtraPartsViewsLacros
    : public ChromeBrowserMainExtraPartsViews {
 public:
  ChromeBrowserMainExtraPartsViewsLacros();
  ChromeBrowserMainExtraPartsViewsLacros(
      const ChromeBrowserMainExtraPartsViewsLacros&) = delete;
  ChromeBrowserMainExtraPartsViewsLacros& operator=(
      const ChromeBrowserMainExtraPartsViewsLacros&) = delete;
  ~ChromeBrowserMainExtraPartsViewsLacros() override;

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PreProfileInit() override;

  std::unique_ptr<FloatControllerLacros> float_controller_;
  std::unique_ptr<ImmersiveContextLacros> immersive_context_;
  std::unique_ptr<SnapControllerLacros> snap_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_LACROS_H_
