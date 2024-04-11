// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_features.h"

#include "base/check_is_test.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/browser.h"

namespace {

// This is the generic entry point for test code to stub out browser window
// functionality. It is called by production code, but only used by tests.
BrowserWindowFeatures::BrowserWindowFeaturesFactory& GetFactory() {
  static base::NoDestructor<BrowserWindowFeatures::BrowserWindowFeaturesFactory>
      factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<BrowserWindowFeatures>
BrowserWindowFeatures::CreateBrowserWindowFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new BrowserWindowFeatures());
}

BrowserWindowFeatures::~BrowserWindowFeatures() = default;

// static
void BrowserWindowFeatures::ReplaceBrowserWindowFeaturesForTesting(
    BrowserWindowFeaturesFactory factory) {
  BrowserWindowFeatures::BrowserWindowFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void BrowserWindowFeatures::Init(Browser* browser) {
  // Avoid passing `browser` directly to features. Instead, pass the minimum
  // necessary state or controllers necessary.
  // Ping erikchen for assistance. This comment will be deleted after there are
  // 10+ features.
  //
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
  }
}

BrowserWindowFeatures::BrowserWindowFeatures() = default;
