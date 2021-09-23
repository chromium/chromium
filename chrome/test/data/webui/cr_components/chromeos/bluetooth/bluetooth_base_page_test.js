// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothBasePageElement} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_base_page.js';
import {ButtonState} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsBluetoothBasePageTest', function() {
  /** @type {?SettingsBluetoothBasePageElement} */
  let bluetoothBasePage;

  setup(function() {
    bluetoothBasePage = /** @type {?SettingsBluetoothBasePageElement} */ (
        document.createElement('bluetooth-base-page'));
    document.body.appendChild(bluetoothBasePage);
    flush();
  });

  /**
   * @param {!HTMLElement} button
   * @return {boolean}
   */
  function isButtonShownAndEnabled(button) {
    return !button.hidden && !button.disabled;
  }

  /**
   * @param {!HTMLElement} button
   * @return {boolean}
   */
  function isButtonShownAndDisabled(button) {
    return !button.hidden && button.disabled;
  }

  /**
   * @param {!ButtonState} state
   */
  function setStateForAllButtons(state) {
    bluetoothBasePage.buttonBarState = {
      cancel: state,
      pair: state,
    };
    flush();
  }

  test('Title is shown', function() {
    const title = bluetoothBasePage.shadowRoot.querySelector('#title');
    assertTrue(!!title);
    assertEquals(
        bluetoothBasePage.i18n('bluetoothPairNewDevice'),
        title.textContent.trim());
  });

  test('Button states', function() {
    const getCancelButton = () =>
        bluetoothBasePage.shadowRoot.querySelector('#cancel');
    const getPairButton = () =>
        bluetoothBasePage.shadowRoot.querySelector('#pair');

    setStateForAllButtons(ButtonState.ENABLED);
    assertTrue(isButtonShownAndEnabled(getCancelButton()));
    assertTrue(isButtonShownAndEnabled(getPairButton()));

    setStateForAllButtons(ButtonState.DISABLED);
    assertTrue(isButtonShownAndDisabled(getCancelButton()));
    assertTrue(isButtonShownAndDisabled(getPairButton()));

    setStateForAllButtons(ButtonState.HIDDEN);
    assertFalse(!!getCancelButton());
    assertFalse(!!getPairButton());
  });
});