// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_FEATURES_H_

#include "base/functional/callback.h"

class Browser;

namespace commerce {
class ProductSpecificationsEntryPointController;
}  // namespace commerce

// This class owns the core controllers for features that are scoped to a given
// browser window on desktop. It can be subclassed by tests to perform
// dependency injection.
class BrowserWindowFeatures {
 public:
  static std::unique_ptr<BrowserWindowFeatures> CreateBrowserWindowFeatures();
  virtual ~BrowserWindowFeatures();

  BrowserWindowFeatures(const BrowserWindowFeatures&) = delete;
  BrowserWindowFeatures& operator=(const BrowserWindowFeatures&) = delete;

  // Call this method to stub out BrowserWindowFeatures for tests.
  using BrowserWindowFeaturesFactory =
      base::RepeatingCallback<std::unique_ptr<BrowserWindowFeatures>()>;
  static void ReplaceBrowserWindowFeaturesForTesting(
      BrowserWindowFeaturesFactory factory);

  // Called exactly once to initialize features.
  void Init(Browser* browser);

  // Public accessors for features, e.g.
  // FooFeature* foo_feature() { return foo_feature_.get(); }
  commerce::ProductSpecificationsEntryPointController*
  product_specifications_entry_point_controller() {
    return product_specifications_entry_point_controller_.get();
  }

 protected:
  BrowserWindowFeatures();

  // Override these methods to stub out individual feature controllers for
  // testing. e.g.
  // virtual std::unique_ptr<FooFeature> CreateFooFeature();

 private:
  // Features that are per-browser window will each have a controller. e.g.
  // std::unique_ptr<FooFeature> foo_feature_;

  std::unique_ptr<commerce::ProductSpecificationsEntryPointController>
      product_specifications_entry_point_controller_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_FEATURES_H_
