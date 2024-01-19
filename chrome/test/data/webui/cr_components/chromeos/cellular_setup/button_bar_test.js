// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/button_bar.js';

import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';

/**
 * @typedef {{
 *   backward: (!ButtonState|undefined),
 *   cancel: (!ButtonState|undefined),
 *   forward: (!ButtonState|undefined),
 * }}
 * Remove typedef once this test is migrated to TS.
 */
let ButtonBarState;

suite('CellularSetupButtonBarTest', function() {
  /** @type {!ButtonBarElement} */
  let buttonBar;
  setup(function() {
    buttonBar = document.createElement('button-bar');
    document.body.appendChild(buttonBar);
    flush();
  });

  teardown(function() {
    buttonBar.remove();
  });

  /**
   * @param {!ButtonBarState} state
   */
  function setStateForAllButtons(state) {
    buttonBar.buttonState = {
      backward: state,
      cancel: state,
      forward: state,
    };
    flush();
  }

  /**
   * @param {!HTMLElement} button
   * @returns {boolean}
   */
  function isButtonShownAndEnabled(button) {
    return !button.hidden && !button.disabled;
  }

  /**
   * @param {!HTMLElement} button
   * @returns {boolean}
   */
  function isButtonShownAndDisabled(button) {
    return !button.hidden && button.disabled;
  }

  /**
   * @param {!HTMLElement} button
   * @returns {boolean}
   */
  function isButtonHidden(button) {
    return !!button.hidden;
  }

  test('individual buttons appear if enabled', function() {
    setStateForAllButtons(ButtonState.ENABLED);
    assertTrue(isButtonShownAndEnabled(
        buttonBar.shadowRoot.querySelector('#backward')));
    assertTrue(
        isButtonShownAndEnabled(buttonBar.shadowRoot.querySelector('#cancel')));
    assertTrue(isButtonShownAndEnabled(
        buttonBar.shadowRoot.querySelector('#forward')));
  });

  test('individual buttons appear but are diabled', function() {
    setStateForAllButtons(ButtonState.DISABLED);
    assertTrue(isButtonShownAndDisabled(
        buttonBar.shadowRoot.querySelector('#backward')));
    assertTrue(isButtonShownAndDisabled(
        buttonBar.shadowRoot.querySelector('#cancel')));
    assertTrue(isButtonShownAndDisabled(
        buttonBar.shadowRoot.querySelector('#forward')));
  });

  test('individual buttons are hidden', function() {
    setStateForAllButtons(ButtonState.HIDDEN);
    assertTrue(isButtonHidden(buttonBar.shadowRoot.querySelector('#backward')));
    assertTrue(isButtonHidden(buttonBar.shadowRoot.querySelector('#cancel')));
    assertTrue(isButtonHidden(buttonBar.shadowRoot.querySelector('#forward')));
  });

  test('default focus is on last button if all are enabled', function() {
    setStateForAllButtons(ButtonState.ENABLED);
    buttonBar.focusDefaultButton();

    flush();

    assertEquals(
        buttonBar.shadowRoot.activeElement,
        buttonBar.shadowRoot.querySelector('#forward'));
  });

  test('default focus is on first button if rest are hidden', function() {
    buttonBar.buttonState = {
      backward: ButtonState.ENABLED,
      cancel: ButtonState.HIDDEN,
      forward: ButtonState.HIDDEN,
    };
    buttonBar.focusDefaultButton();

    flush();

    assertEquals(
        buttonBar.shadowRoot.activeElement,
        buttonBar.shadowRoot.querySelector('#backward'));
  });

  test(
      'default focus is on first button if rest are visible but disabled',
      function() {
        buttonBar.buttonState = {
          backward: ButtonState.ENABLED,
          cancel: ButtonState.DISABLED,
          forward: ButtonState.DISABLED,
        };
        buttonBar.focusDefaultButton();

        flush();

        assertEquals(
            buttonBar.shadowRoot.activeElement,
            buttonBar.shadowRoot.querySelector('#backward'));
      });
});
