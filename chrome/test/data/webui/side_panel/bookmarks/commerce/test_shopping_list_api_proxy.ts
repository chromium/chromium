// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarkProductInfo} from 'chrome://read-later.top-chrome/bookmarks/commerce/shopping_list.mojom-webui.js';
import {ShoppingListApiProxy} from 'chrome://read-later.top-chrome/bookmarks/commerce/shopping_list_api_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestShoppingListApiProxy extends TestBrowserProxy implements
    ShoppingListApiProxy {
  private products_: BookmarkProductInfo[] = [];

  constructor() {
    super(['getAllBookmarkProductInfo']);
  }

  setProducts(products: BookmarkProductInfo[]) {
    this.products_ = products;
  }

  getAllBookmarkProductInfo() {
    this.methodCalled('getAllBookmarkProductInfo');
    return Promise.resolve({productInfos: this.products_});
  }
}
