// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.js';

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

  textChanged(newText) {
    this.methodCalled('textChanged', newText);
  }

  optionSelected(index, resultSetId) {
    this.methodCalled('optionSelected', [index, resultSetId]);
  }

  heightChanged(newHeight) {
    this.methodCalled('heightChanged', newHeight);
  }

  dismiss() {
    this.methodCalled('dismiss');
  }

  promptCancelled() {
    this.methodCalled('promptCancelled');
  }
}
