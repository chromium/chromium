// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {SetTimeBrowserProxy} */
export class TestSetTimeBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'sendPageReady',
      'setTimeInSeconds',
      'setTimezone',
      'dialogClose',
      'doneClicked',
    ]);
  }

  /** @override */
  sendPageReady() {
    this.methodCalled('sendPageReady');
  }

  /** @override */
  setTimeInSeconds(timeInSeconds) {
    this.methodCalled('setTimeInSeconds', timeInSeconds);
  }

  /** @override */
  setTimezone(timezone) {
    this.methodCalled('setTimezone', timezone);
  }

  /** @override */
  dialogClose() {
    this.methodCalled('dialogClose');
  }

  /** @override */
  doneClicked() {
    this.methodCalled('doneClicked');
  }
}
