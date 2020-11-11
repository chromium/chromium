// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://commander/browser_proxy.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {BrowserProxy} */
export class TestCommanderBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'textChanged',
      'optionSelected',
      'heightChanged',
      'dismiss',
      'promptCancelled',
    ]);
  }

  /** @override */
  textChanged(newText) {
    this.methodCalled('textChanged', newText);
  }

  /** @override */
  optionSelected(index, resultSetId) {
    this.methodCalled('optionSelected', [index, resultSetId]);
  }

  /** @override */
  heightChanged(newHeight) {
    this.methodCalled('heightChanged', newHeight);
  }

  /** @override */
  dismiss() {
    this.methodCalled('dismiss');
  }

  /** @override */
  promptCancelled() {
    this.methodCalled('promptCancelled');
  }
}
