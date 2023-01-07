// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://commander/browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestCommanderBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  constructor() {
    super([
      'textChanged',
      'optionSelected',
      'heightChanged',
      'dismiss',
      'promptCancelled',
    ]);
  }

  textChanged(newText: string) {
    this.methodCalled('textChanged', newText);
  }

  optionSelected(index: number, resultSetId: number) {
    this.methodCalled('optionSelected', [index, resultSetId]);
  }

  heightChanged(newHeight: number) {
    this.methodCalled('heightChanged', newHeight);
  }

  dismiss() {
    this.methodCalled('dismiss');
  }

  promptCancelled() {
    this.methodCalled('promptCancelled');
  }
}
