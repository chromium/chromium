// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IntroBrowserProxy} from 'chrome://intro/browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';

export class TestIntroBrowserProxy extends TestBrowserProxy implements
    IntroBrowserProxy {
  constructor() {
    super([
      'continueWithAccount',
      'continueWithoutAccount',
      'initializeMainView',
    ]);
  }

  private mediaQueryList_: FakeMediaQueryList = new FakeMediaQueryList('dummy');

  continueWithAccount() {
    this.methodCalled('continueWithAccount');
  }

  continueWithoutAccount() {
    this.methodCalled('continueWithoutAccount');
  }

  initializeMainView() {
    this.methodCalled('initializeMainView');
  }

  matchMedia(_query: string): MediaQueryList {
    return this.mediaQueryList_;
  }

  setMatchMediaMatches(matches: boolean): void {
    this.mediaQueryList_.matches = matches;
  }
}
