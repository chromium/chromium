// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/button_bar.js';

import {ButtonBarState, ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../../chromeos/chai_assert.js';

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
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#backward')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#cancel')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#forward')));
  });

  test('individual buttons appear but are diabled', function() {
    setStateForAllButtons(ButtonState.DISABLED);
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#backward')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#cancel')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#forward')));
  });

  test('individual buttons are hidden', function() {
    setStateForAllButtons(ButtonState.HIDDEN);
    assertTrue(isButtonHidden(buttonBar.$$('#backward')));
    assertTrue(isButtonHidden(buttonBar.$$('#cancel')));
    assertTrue(isButtonHidden(buttonBar.$$('#forward')));
  });

  test('default focus is on last button if all are enabled', function() {
    setStateForAllButtons(ButtonState.ENABLED);
    buttonBar.focusDefaultButton();

    flush();

    assertEquals(buttonBar.shadowRoot.activeElement, buttonBar.$$('#forward'));
  });

  test('default focus is on first button if rest are hidden', function() {
    buttonBar.buttonState = {
      backward: ButtonState.ENABLED,
      cancel: ButtonState.HIDDEN,
      forward: ButtonState.HIDDEN,
    };
    buttonBar.focusDefaultButton();

    flush();

    assertEquals(buttonBar.shadowRoot.activeElement, buttonBar.$$('#backward'));
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
            buttonBar.shadowRoot.activeElement, buttonBar.$$('#backward'));
      });
});
