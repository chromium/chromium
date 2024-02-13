// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PluralStringProxy. */

// clang-format off
import type {PluralStringProxy} from 'chrome://resources/js/plural_string_proxy.js';

import {TestBrowserProxy} from './test_browser_proxy.js';
// clang-format on

/**
 * Test implementation
 */
export class TestPluralStringProxy extends TestBrowserProxy implements
    PluralStringProxy {
  text: string = 'some text';
  constructor() {
    super([
      'getPluralString',
    ]);
  }

  getPluralString(messageName: string, itemCount: number) {
    this.methodCalled('getPluralString', {messageName, itemCount});
    return Promise.resolve(this.text);
  }

  getPluralStringTupleWithComma(
      messageName1: string, itemCount1: number, messageName2: string,
      itemCount2: number) {
    this.methodCalled(
        'getPluralStringTupleWithComma',
        {messageName1, itemCount1, messageName2, itemCount2});
    return Promise.resolve(this.text);
  }

  getPluralStringTupleWithPeriods(
      messageName1: string, itemCount1: number, messageName2: string,
      itemCount2: number) {
    this.methodCalled(
        'getPluralStringTupleWithPeriods',
        {messageName1, itemCount1, messageName2, itemCount2});
    return Promise.resolve(this.text);
  }
}
