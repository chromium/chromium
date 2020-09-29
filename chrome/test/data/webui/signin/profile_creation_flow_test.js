// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-picker/lazy_load.js';

import {assertTrue} from '../chai_assert.js';
import {isChildVisible} from '../test_util.m.js';

suite('ProfileCreationFlowTest', function() {
  /** @type {!ProfileTypeChoiceElement} */
  let choice;

  setup(function() {
    document.body.innerHTML = '';
    choice = /** @type {!ProfileTypeChoiceElement} */(
        document.createElement('profile-type-choice'));
    document.body.append(choice);
  });

  test('BackButton', function() {
    assertTrue(isChildVisible(choice, '#backButton'));
  });

  test('SignInButton', function() {
    assertTrue(isChildVisible(choice, '#signInButton'));
  });

  test('NotNowButton', function() {
    assertTrue(isChildVisible(choice, '#notNowButton'));
  });
});
