// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMERCE_SHOPPING_COLLECTION_IPH_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_COMMERCE_SHOPPING_COLLECTION_IPH_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"

namespace commerce {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kShoppingCollectionIPHViewId);

class ShoppingCollectionIphView : public views::View {
 public:
  ShoppingCollectionIphView();
  ShoppingCollectionIphView(const ShoppingCollectionIphView&) = delete;
  ShoppingCollectionIphView& operator=(const ShoppingCollectionIphView&) =
      delete;
  ~ShoppingCollectionIphView() override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_VIEWS_COMMERCE_SHOPPING_COLLECTION_IPH_VIEW_H_
