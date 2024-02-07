// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkManagerApiProxy} from 'chrome://bookmarks/bookmarks.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarkManagerApiProxy extends TestBrowserProxy implements
    BookmarkManagerApiProxy {
  onDragEnter = new FakeChromeEvent();
  private canPaste_ = false;

  constructor() {
    super([
      'canPaste',
      'copy',
      'cut',
      'drop',
      'openInNewTab',
      'openInNewWindow',
      'paste',
      'removeTrees',
      'startDrag',
    ]);
  }

  setCanPaste(canPaste: boolean) {
    this.canPaste_ = canPaste;
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

  removeTrees(idList: string[]) {
    this.methodCalled('removeTrees', idList);
    return Promise.resolve();
  }

  canPaste(_parentId: string) {
    this.methodCalled('canPaste');
    return Promise.resolve(this.canPaste_);
  }

  openInNewWindow(idList: string[], incognito: boolean) {
    this.methodCalled('openInNewWindow', [idList, incognito]);
  }

  openInNewTab(id: string, active: boolean) {
    this.methodCalled('openInNewTab', [id, active]);
  }

  cut(idList: string[]) {
    this.methodCalled('cut', idList);
    return Promise.resolve();
  }

  paste(parentId: string, _selectedIdList?: string[]) {
    this.methodCalled('paste', parentId);
    return Promise.resolve();
  }

  copy(idList: string[]) {
    this.methodCalled('copy', idList);
    return Promise.resolve();
  }
}
