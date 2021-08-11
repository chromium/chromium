// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccountSelectionLacrosElement} from 'chrome://profile-picker/lazy_load.js';

import {assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

suite('ProfileTypeChoiceTest', function() {
  /** @type {!AccountSelectionLacrosElement} */
  let testElement;

  setup(function() {
    document.body.innerHTML = '';
    testElement = /** @type {!AccountSelectionLacrosElement} */ (
        document.createElement('account-selection-lacros'));
    document.body.append(testElement);
  });

  test('BackButton', function() {
    assertTrue(isChildVisible(testElement, '#backButton'));
  });
});
