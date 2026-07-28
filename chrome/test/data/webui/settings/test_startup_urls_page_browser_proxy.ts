// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {StartupUrlsPageBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestStartupUrlsPageBrowserProxy extends TestBrowserProxy implements
    StartupUrlsPageBrowserProxy {
  private urlIsValid_: boolean = true;

  constructor() {
    super([
      'addStartupPage',
      'editStartupPage',
      'loadStartupPages',
      'removeStartupPage',
      'useCurrentPages',
      'validateStartupPage',
    ]);
  }

  setUrlValidity(isValid: boolean) {
    this.urlIsValid_ = isValid;
  }

  addStartupPage(url: string) {
    this.methodCalled('addStartupPage', url);
    return Promise.resolve(this.urlIsValid_);
  }

  editStartupPage(modelIndex: number, url: string) {
    this.methodCalled('editStartupPage', [modelIndex, url]);
    return Promise.resolve(this.urlIsValid_);
  }

  loadStartupPages() {
    this.methodCalled('loadStartupPages');
  }

  removeStartupPage(modelIndex: number) {
    this.methodCalled('removeStartupPage', modelIndex);
  }

  useCurrentPages() {
    this.methodCalled('useCurrentPages');
  }

  validateStartupPage(url: string) {
    this.methodCalled('validateStartupPage', url);
    return Promise.resolve(this.urlIsValid_);
  }
}
