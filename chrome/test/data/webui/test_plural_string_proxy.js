// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PluralStringProxy. */

// clang-format off
import {PluralStringProxy} from 'chrome://resources/js/plural_string_proxy.js';
import {TestBrowserProxy} from './test_browser_proxy.m.js';
// clang-format on

/**
 * Test implementation
 * @implements {PluralStringProxy}
 */
export class TestPluralStringProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getPluralString',
    ]);

    this.text = 'some text';
  }

  /** override */
  getPluralString(messageName, itemCount) {
    this.methodCalled('getPluralString', {messageName, itemCount});
    return Promise.resolve(this.text);
  }

  /** @override */
  getPluralStringTupleWithComma(
      messageName1, itemCount1, messageName2, itemCount2) {
    this.methodCalled(
        'getPluralStringTupleWithComma',
        {messageName1, itemCount1, messageName2, itemCount2});
    return Promise.resolve(this.text);
  }

  /** @override */
  getPluralStringTupleWithPeriods(
      messageName1, itemCount1, messageName2, itemCount2) {
    this.methodCalled(
        'getPluralStringTupleWithPeriods',
        {messageName1, itemCount1, messageName2, itemCount2});
    return Promise.resolve(this.text);
  }
}
