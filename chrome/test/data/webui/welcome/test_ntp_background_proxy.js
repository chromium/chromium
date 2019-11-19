// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {NtpBackgroundProxy} */
export class TestNtpBackgroundProxy extends TestBrowserProxy {
  constructor() {
    super([
      'clearBackground',
      'getBackgrounds',
      'preloadImage',
      'recordBackgroundImageFailedToLoad',
      'recordBackgroundImageLoadTime',
      'setBackground',
    ]);

    /** @private {!Array<!NtpBackgroundData} */
    this.backgroundsList_ = [];

    /** @private {boolean} */
    this.preloadImageSuccess_ = true;
  }

  /** @override */
  clearBackground() {
    this.methodCalled('clearBackground');
  }

  /** @override */
  getBackgrounds() {
    this.methodCalled('getBackgrounds');
    return Promise.resolve(this.backgroundsList_);
  }

  /** @override */
  preloadImage(url) {
    this.methodCalled('preloadImage');
    return this.preloadImageSuccess_ ? Promise.resolve() : Promise.reject();
  }

  /** @override */
  recordBackgroundImageFailedToLoad() {
    this.methodCalled('recordBackgroundImageFailedToLoad');
  }

  /** @override */
  recordBackgroundImageLoadTime(loadTime) {
    this.methodCalled('recordBackgroundImageLoadTime', loadTime);
  }

  /** @override */
  setBackground(id) {
    this.methodCalled('setBackground', id);
  }

  /** @param {boolean} success */
  setPreloadImageSuccess(success) {
    this.preloadImageSuccess_ = success;
  }

  /** @param {!Array<!NtpBackgroundData>} backgroundsList */
  setBackgroundsList(backgroundsList) {
    this.backgroundsList_ = backgroundsList;
  }
}
