// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/cellular_setup/confirmation_code_page_legacy.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';

import {assertTrue} from '../../../chromeos/chai_assert.js';

suite('CrComponentsConfirmationCodePageLegacyTest', function() {
  let confirmationCodePageLegacy;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    confirmationCodePageLegacy =
        document.createElement('confirmation-code-page-legacy');
    document.body.appendChild(confirmationCodePageLegacy);
    flush();
  });

  test('Event is fired when enter is pressed on input', async function() {
    await flushAsync();
    let eventFired = false;
    confirmationCodePageLegacy.addEventListener(
        'forward-navigation-requested', () => {
          eventFired = true;
        });
    const input = confirmationCodePageLegacy.$$('#confirmationCode');
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    await flushAsync();
    assertTrue(eventFired);
  });

  test('displays profile name', async function() {
    const detailsElement =
        confirmationCodePageLegacy.shadowRoot.getElementById('details');
    assertEquals(
        '', detailsElement.textContent.trim(),
        'no profile name is shown without profileProperties');

    const fakeESimManagerRemote = new FakeESimManagerRemote();
    const fakeEuicc = fakeESimManagerRemote.addEuiccForTest(1);
    const {profiles: [fakeProfile]} = await fakeEuicc.getProfileList();

    confirmationCodePageLegacy.profile = fakeProfile;
    await flushAsync();

    assertEquals(
        'profile1', detailsElement.textContent.trim(),
        'correct profile name is shown');
  });
});
