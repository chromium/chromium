// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"

namespace commerce {

// TODO(b/325661685): Add implementation for
// ProductSpecificationsEntryPointController.
ProductSpecificationsEntryPointController::
    ProductSpecificationsEntryPointController(TabStripModel* tab_strip_model)
    : tab_strip_model_(tab_strip_model) {
  CHECK(tab_strip_model_);
  tab_strip_model_->AddObserver(this);
}

ProductSpecificationsEntryPointController::
    ~ProductSpecificationsEntryPointController() {
  CHECK(tab_strip_model_);
  tab_strip_model_->RemoveObserver(this);
}
}  // namespace commerce
