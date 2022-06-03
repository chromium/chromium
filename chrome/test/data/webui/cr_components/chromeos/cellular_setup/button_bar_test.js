// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/button_bar.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {ButtonState, Button, ButtonBarState, CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CellularSetupButtonBarTest', function() {
  /** @type {!ButtonBarElement} */
  let buttonBar;
  setup(function() {
    buttonBar = document.createElement('button-bar');
    document.body.appendChild(buttonBar);
    Polymer.dom.flush();
  });

  teardown(function() {
    buttonBar.remove();
  });

  /**
   * @param {!cellularSetup.ButtonBarState} state
   */
  function setStateForAllButtons(state) {
    buttonBar.buttonState = {
      backward: state,
      cancel: state,
      forward: state,
    };
    Polymer.dom.flush();
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
    setStateForAllButtons(cellularSetup.ButtonState.ENABLED);
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#backward')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#cancel')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#forward')));
  });

  test('individual buttons appear but are diabled', function() {
    setStateForAllButtons(cellularSetup.ButtonState.DISABLED);
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#backward')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#cancel')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#forward')));
  });

  test('individual buttons are hidden', function() {
    setStateForAllButtons(cellularSetup.ButtonState.HIDDEN);
    assertTrue(isButtonHidden(buttonBar.$$('#backward')));
    assertTrue(isButtonHidden(buttonBar.$$('#cancel')));
    assertTrue(isButtonHidden(buttonBar.$$('#forward')));
  });

  test('default focus is on last button if all are enabled', function() {
    setStateForAllButtons(cellularSetup.ButtonState.ENABLED);
    buttonBar.focusDefaultButton();

    Polymer.dom.flush();

    assertEquals(buttonBar.shadowRoot.activeElement, buttonBar.$$('#forward'));
  });

  test('default focus is on first button if rest are hidden', function() {
    buttonBar.buttonState = {
      backward: cellularSetup.ButtonState.ENABLED,
      cancel: cellularSetup.ButtonState.HIDDEN,
      forward: cellularSetup.ButtonState.HIDDEN,
    };
    buttonBar.focusDefaultButton();

    Polymer.dom.flush();

    assertEquals(buttonBar.shadowRoot.activeElement, buttonBar.$$('#backward'));
  });

  test(
      'default focus is on first button if rest are visible but disabled',
      function() {
        buttonBar.buttonState = {
          backward: cellularSetup.ButtonState.ENABLED,
          cancel: cellularSetup.ButtonState.DISABLED,
          forward: cellularSetup.ButtonState.DISABLED,
        };
        buttonBar.focusDefaultButton();

        Polymer.dom.flush();

        assertEquals(
            buttonBar.shadowRoot.activeElement, buttonBar.$$('#backward'));
      });
});
