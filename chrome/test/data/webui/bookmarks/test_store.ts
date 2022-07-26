// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createEmptyState, reduceAction, Store} from 'chrome://bookmarks/bookmarks.js';
import {TestStore as CrUiTestStore} from 'chrome://webui-test/test_store.js';

export class TestStore extends CrUiTestStore {
  constructor(data: any) {
    super(data, Store, createEmptyState(), reduceAction);
  }

  override replaceSingleton() {
    Store.setInstance(this);
  }
}
