// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

#include "base/logging.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/shopping_bookmark_model_observer.h"

namespace commerce {

ShoppingService::ShoppingService(bookmarks::BookmarkModel* bookmark_model) {
  if (bookmark_model) {
    shopping_bookmark_observer_ =
        std::make_unique<ShoppingBookmarkModelObserver>(bookmark_model);
  }
}

void ShoppingService::RegisterPrefs(PrefRegistrySimple* registry) {
  // This pref value is queried from server. Set initial value as true so our
  // features can be correctly set up while waiting for the server response.
  registry->RegisterBooleanPref(commerce::kWebAndAppActivityEnabledForShopping,
                                true);
}

void ShoppingService::Shutdown() {}

ShoppingService::~ShoppingService() = default;

}  // namespace commerce
