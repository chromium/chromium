// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/button_bar.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {ButtonState, Button, ButtonBarState, CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// #import {assertFalse, assertTrue} from '../../../chai_assert.js';
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
      next: state,
      tryAgain: state,
      done: state,
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
    setStateForAllButtons(cellularSetup.ButtonState.SHOWN_AND_ENABLED);
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#backward')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#cancel')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#tryAgain')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#next')));
    assertTrue(isButtonShownAndEnabled(buttonBar.$$('#done')));
  });

  test('individual buttons appear but are diabled', function() {
    setStateForAllButtons(cellularSetup.ButtonState.SHOWN_BUT_DISABLED);
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#backward')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#cancel')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#tryAgain')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#next')));
    assertTrue(isButtonShownAndDisabled(buttonBar.$$('#done')));
  });

  test('individual buttons are hidden', function() {
    setStateForAllButtons(cellularSetup.ButtonState.HIDDEN);
    assertTrue(isButtonHidden(buttonBar.$$('#backward')));
    assertTrue(isButtonHidden(buttonBar.$$('#cancel')));
    assertTrue(isButtonHidden(buttonBar.$$('#tryAgain')));
    assertTrue(isButtonHidden(buttonBar.$$('#next')));
    assertTrue(isButtonHidden(buttonBar.$$('#done')));
  });
});
