// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarkManagerApiProxy} from 'chrome://bookmarks/bookmarks.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarkManagerApiProxy extends TestBrowserProxy implements
    BookmarkManagerApiProxy {
  onDragEnter = new FakeChromeEvent();

  constructor() {
    super([
      'drop',
      'startDrag',
      'removeTrees',
    ]);
  }

  drop(parentId: string, index?: number) {
    this.methodCalled('drop', [parentId, index]);
    return Promise.resolve();
  }

  startDrag(
      idList: string[], _dragNodeIndex: number, _isFromTouch: boolean,
      _x: number, _y: number) {
    this.methodCalled('startDrag', idList);
  }

  removeTrees(_idList: string[]) {
    this.methodCalled('removeTrees');
    return Promise.resolve();
  }
}
