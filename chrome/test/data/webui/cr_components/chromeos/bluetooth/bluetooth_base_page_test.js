// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';

import {SettingsBluetoothBasePageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_base_page.js';
import {ButtonState} from 'chrome://resources/ash/common/bluetooth/bluetooth_types.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../../chromeos/chai_assert.js';
import {eventToPromise} from '../../../chromeos/test_util.js';

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

  test('Title and loading indicator are shown', function() {
    const title = bluetoothBasePage.shadowRoot.querySelector('#title');
    assertTrue(!!title);
    assertEquals(
        bluetoothBasePage.i18n('bluetoothPairNewDevice'),
        title.textContent.trim());

    const getProgress = () =>
        bluetoothBasePage.shadowRoot.querySelector('paper-progress');
    assertFalse(!!getProgress());
    bluetoothBasePage.showScanProgress = true;
    flush();
    assertTrue(!!getProgress());
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

  test('default focus, and aria description', async function() {
    bluetoothBasePage.focusDefault = true;
    bluetoothBasePage.buttonBarState = {
      cancel: ButtonState.DISABLED,
      pair: ButtonState.ENABLED,
    };
    await waitAfterNextRender(bluetoothBasePage);
    const pairButton = bluetoothBasePage.shadowRoot.querySelector('#pair');
    assertEquals(getDeepActiveElement(), pairButton);
  });

  test('Cancel and pair events fired on click', async function() {
    const getCancelButton = () =>
        bluetoothBasePage.shadowRoot.querySelector('#cancel');
    const getPairButton = () =>
        bluetoothBasePage.shadowRoot.querySelector('#pair');

    setStateForAllButtons(ButtonState.ENABLED);

    const cancelEventPromise = eventToPromise('cancel', bluetoothBasePage);
    const pairEventPromise = eventToPromise('pair', bluetoothBasePage);

    getCancelButton().click();
    await cancelEventPromise;

    getPairButton().click();
    await pairEventPromise;
  });
});
