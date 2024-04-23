// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"

namespace commerce {

class ProductSpecificationsEntryPointController : public TabStripModelObserver {
 public:
  explicit ProductSpecificationsEntryPointController(
      TabStripModel* tab_strip_model);
  ~ProductSpecificationsEntryPointController() override;

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};
}  // namespace commerce

#endif  // CHROME_BROWSER_UI_COMMERCE_PRODUCT_SPECIFICATIONS_ENTRY_POINT_CONTROLLER_H_
