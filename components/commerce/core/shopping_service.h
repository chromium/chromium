// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace commerce {

class ShoppingBookmarkModelObserver;

class ShoppingService : public KeyedService, public base::SupportsUserData {
 public:
  explicit ShoppingService(bookmarks::BookmarkModel* bookmark_model);
  ~ShoppingService() override;

  ShoppingService(const ShoppingService&) = delete;
  ShoppingService& operator=(const ShoppingService&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Shutdown() override;

 private:
  // The service's means of observing the bookmark model which is automatically
  // removed from the model when destroyed. This will be null if no
  // BookmarkModel is provided to the service.
  std::unique_ptr<ShoppingBookmarkModelObserver> shopping_bookmark_observer_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
