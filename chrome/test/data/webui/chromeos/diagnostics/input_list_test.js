// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/input_list.js';

import {fakeKeyboards, fakeTouchDevices} from 'chrome://diagnostics/fake_data.js';
import {FakeInputDataProvider} from 'chrome://diagnostics/fake_input_data_provider.js';
import {setInputDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function inputListTestSuite() {
  /** @type {?InputListElement} */
  let inputListElement = null;

  /** @type {?FakeInputDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeInputDataProvider();
    setInputDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    inputListElement.remove();
    inputListElement = null;
  });

  function initializeInputList() {
    assertFalse(!!inputListElement);
    provider.setFakeConnectedDevices(fakeKeyboards, fakeTouchDevices);

    // Add the input list to the DOM.
    inputListElement =
        /** @type {!InputListElement} */ (document.createElement('input-list'));
    assertTrue(!!inputListElement);
    document.body.appendChild(inputListElement);

    return flushTasks();
  }

  test('InputListPopulated', () => {
    return initializeInputList().then(() => {
      dx_utils.assertElementContainsText(
          inputListElement.$$('#keyboardList'), fakeKeyboards[0].name);
      dx_utils.assertElementContainsText(
          inputListElement.$$('#touchpadList'), fakeTouchDevices[0].name);
      dx_utils.assertElementContainsText(
          inputListElement.$$('#touchscreenList'), fakeTouchDevices[1].name);
    });
  });
}
