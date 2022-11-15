// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementations of ChromeEvent.
 */

import {assertFalse, assertTrue} from './chai_assert.js';

export class FakeChromeEvent {
  private listeners_: Set<Function> = new Set();

  addListener(listener: Function) {
    assertFalse(
        this.listeners_.has(listener),
        'FakeChromeEvent.addListened: Listener already added');
    this.listeners_.add(listener);
  }

  removeListener(listener: Function) {
    assertTrue(
        this.listeners_.has(listener),
        'FakeChromeEvent.removeListener: Listener does not exist');
    this.listeners_.delete(listener);
  }

  callListeners(...args: any[]) {
    this.listeners_.forEach(function(l) {
      l(...args);
    });
  }
}
