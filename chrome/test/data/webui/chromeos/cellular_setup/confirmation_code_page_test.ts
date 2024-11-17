// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/confirmation_code_page.js';

import type {ConfirmationCodePageElement} from 'chrome://resources/ash/common/cellular_setup/confirmation_code_page.js';
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrComponentsConfirmationCodePageTest', function() {
  let confirmationCodePage: ConfirmationCodePageElement;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    confirmationCodePage = document.createElement('confirmation-code-page');
    document.body.appendChild(confirmationCodePage);
    flush();
  });

  test('Event is fired when enter is pressed on input', async function() {
    await flushAsync();
    let eventFired = false;
    confirmationCodePage.addEventListener(
        'forward-navigation-requested', () => {
          eventFired = true;
        });
    const input =
        confirmationCodePage.shadowRoot!.querySelector('#confirmationCode');
    assertTrue(!!input);
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    await flushAsync();
    assertTrue(eventFired);
  });

  test('displays profile name', async function() {
    const detailsElement =
        confirmationCodePage.shadowRoot!.getElementById('details');
    assertTrue(!!detailsElement);
    assertEquals(
        '', detailsElement.textContent!.trim(),
        'no profile name is shown without profileProperties');

    confirmationCodePage.profileProperties = {
      eid: 'eid',
      iccid: '1',
      name: stringToMojoString16('test profile name'),
      nickname: stringToMojoString16('test profile nickname'),
      serviceProvider: stringToMojoString16('test profile provider'),
      state: ProfileState.kActive,
      activationCode: 'test activation code',
    };

    assertEquals(
        'test profile name', detailsElement.textContent!.trim(),
        'correct profile name is shown');
  });
});
