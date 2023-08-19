// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/confirmation_code_page.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertTrue} from '../../../chromeos/chai_assert.js';

suite('CrComponentsConfirmationCodePageTest', function() {
  let confirmationCodePage;

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
    const input = confirmationCodePage.$$('#confirmationCode');
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    await flushAsync();
    assertTrue(eventFired);
  });
});
