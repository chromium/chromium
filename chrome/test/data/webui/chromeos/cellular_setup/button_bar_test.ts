// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/button_bar.js';

import type {ButtonBarElement} from 'chrome://resources/ash/common/cellular_setup/button_bar.js';
import {ButtonState} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CellularSetupButtonBarTest', function() {
  let buttonBar: ButtonBarElement;

  setup(function() {
    buttonBar = document.createElement('button-bar');
    document.body.appendChild(buttonBar);
    flush();
  });

  teardown(function() {
    buttonBar.remove();
  });

  function setStateForAllButtons(state: ButtonState): void {
    buttonBar.buttonState = {
      cancel: state,
      forward: state,
    };
    flush();
  }

  function isButtonShownAndEnabled(button: CrButtonElement): boolean {
    return !button.hidden && !button.disabled;
  }

  function isButtonShownAndDisabled(button: CrButtonElement): boolean {
    return !button.hidden && button.disabled;
  }

  function isButtonHidden(button: CrButtonElement): boolean {
    return !!button.hidden;
  }

  test('individual buttons appear if enabled', function() {
    setStateForAllButtons(ButtonState.ENABLED);

    const cancel =
        buttonBar.shadowRoot!.querySelector<CrButtonElement>('#cancel');
    const forward =
        buttonBar.shadowRoot!.querySelector<CrButtonElement>('#forward');
    assertTrue(!!cancel);
    assertTrue(!!forward);
    assertTrue(isButtonShownAndEnabled(cancel));
    assertTrue(isButtonShownAndEnabled(forward));
  });

  test('individual buttons appear but are disabled', function() {
    setStateForAllButtons(ButtonState.DISABLED);

    const cancel =
        buttonBar.shadowRoot!.querySelector<CrButtonElement>('#cancel');
    const forward =
        buttonBar.shadowRoot!.querySelector<CrButtonElement>('#forward');
    assertTrue(!!cancel);
    assertTrue(!!forward);
    assertTrue(isButtonShownAndDisabled(cancel));
    assertTrue(isButtonShownAndDisabled(forward));
  });

  test('individual buttons are hidden', function() {
    setStateForAllButtons(ButtonState.HIDDEN);
    const cancel =
        buttonBar.shadowRoot!.querySelector<CrButtonElement>('#cancel');
    const forward =
        buttonBar.shadowRoot!.querySelector<CrButtonElement>('#forward');
    assertTrue(!!cancel);
    assertTrue(!!forward);
    assertTrue(isButtonHidden(cancel));
    assertTrue(isButtonHidden(forward));
  });

  test('default focus is on last button if all are enabled', function() {
    setStateForAllButtons(ButtonState.ENABLED);
    buttonBar.focusDefaultButton();

    flush();

    assertEquals(
        buttonBar.shadowRoot!.activeElement,
        buttonBar.shadowRoot!.querySelector('#forward'));
  });

  test('default focus is on first button if rest are hidden', function() {
    buttonBar.buttonState = {
      cancel: ButtonState.ENABLED,
      forward: ButtonState.HIDDEN,
    };
    buttonBar.focusDefaultButton();

    flush();

    assertEquals(
        buttonBar.shadowRoot!.activeElement,
        buttonBar.shadowRoot!.querySelector('#cancel'));
  });

  test(
      'default focus is on first button if rest are visible but disabled',
      function() {
        buttonBar.buttonState = {
          cancel: ButtonState.ENABLED,
          forward: ButtonState.DISABLED,
        };
        buttonBar.focusDefaultButton();

        flush();

        assertEquals(
            buttonBar.shadowRoot!.activeElement,
            buttonBar.shadowRoot!.querySelector('#cancel'));
      });
});
