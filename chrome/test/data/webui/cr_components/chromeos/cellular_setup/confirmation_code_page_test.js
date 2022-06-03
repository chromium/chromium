// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/confirmation_code_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsConfirmationCodePageTest', function() {
  let confirmationCodePage;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    confirmationCodePage = document.createElement('confirmation-code-page');
    document.body.appendChild(confirmationCodePage);
    Polymer.dom.flush();
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
