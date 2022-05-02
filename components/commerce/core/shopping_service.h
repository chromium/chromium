// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;

namespace commerce {

class ShoppingService : public KeyedService, public base::SupportsUserData {
 public:
  ShoppingService() = default;
  ~ShoppingService() override = default;

  ShoppingService(const ShoppingService&) = delete;
  ShoppingService& operator=(const ShoppingService&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Shutdown() override;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_SERVICE_H_
