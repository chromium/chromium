// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksPageState} from 'chrome://bookmarks/bookmarks.js';
import {createEmptyState, reduceAction, Store} from 'chrome://bookmarks/bookmarks.js';
import {TestStore as CrUiTestStore} from 'chrome://webui-test/test_store.js';

export class TestStore extends CrUiTestStore<BookmarksPageState> {
  constructor(data: any) {
    super(data as BookmarksPageState, createEmptyState(), reduceAction);
  }

  replaceSingleton() {
    Store.setInstance(this);
  }
}
