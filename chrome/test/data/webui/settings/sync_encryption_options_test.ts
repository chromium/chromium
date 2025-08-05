// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrInputElement, SettingsSyncEncryptionOptionsElement} from 'chrome://settings/lazy_load.js';
import type {CrButtonElement, CrRadioButtonElement, CrRadioGroupElement} from 'chrome://settings/settings.js';
import {SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getSyncAllPrefs} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';


suite('SyncEncryptionOptions', function() {
  let encryptionElement: SettingsSyncEncryptionOptionsElement;
  let testSyncBrowserProxy: TestSyncBrowserProxy;
  let encryptionRadioGroup: CrRadioGroupElement;
  let encryptWithGoogle: CrRadioButtonElement;
  let encryptWithPassphrase: CrRadioButtonElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);

    encryptionElement =
        document.createElement('settings-sync-encryption-options');
    document.body.appendChild(encryptionElement);

    encryptionElement.syncPrefs = getSyncAllPrefs();
    testSyncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };

    await waitBeforeNextRender(encryptionElement);
    assertTrue(!!encryptionElement, 'encryptionElement');

    encryptionRadioGroup =
        encryptionElement.shadowRoot!.querySelector('#encryptionRadioGroup')!;
    encryptWithGoogle = encryptionElement.shadowRoot!.querySelector(
        'cr-radio-button[name="encrypt-with-google"]')!;
    encryptWithPassphrase = encryptionElement.shadowRoot!.querySelector(
        'cr-radio-button[name="encrypt-with-passphrase"]')!;
    assertTrue(!!encryptionRadioGroup, 'encryptionRadioGroup');
    assertTrue(!!encryptWithGoogle, 'encryptWithGoogle');
    assertTrue(!!encryptWithPassphrase, 'encryptWithPassphrase');

    return microtasksFinished();
  });

  test('RadioBoxesEnabledWhenUnencrypted', async () => {
    // Verify that the encryption radio boxes are enabled.
    assertFalse(encryptionRadioGroup.disabled);
    assertEquals(encryptWithGoogle.getAttribute('aria-disabled'), 'false');
    assertEquals(encryptWithPassphrase.getAttribute('aria-disabled'), 'false');

    assertTrue(encryptWithGoogle.checked);

    // Select 'Encrypt with passphrase' to create a new passphrase.
    assertFalse(
        !!encryptionElement.shadowRoot!.querySelector('#create-password-box'));

    encryptWithPassphrase.click();
    await eventToPromise('selected-changed', encryptionRadioGroup);

    assertTrue(
        !!encryptionElement.shadowRoot!.querySelector('#create-password-box'));
    const saveNewPassphrase =
        encryptionElement.shadowRoot!.querySelector<CrButtonElement>(
            '#saveNewPassphrase');
    assertTrue(!!saveNewPassphrase);

    // Test that a sync prefs update does not reset the selection.
    encryptionElement.syncPrefs = getSyncAllPrefs();
    // Wait for a timeout so that the check below catches any incorrect reset.
    await microtasksFinished();
    assertTrue(encryptWithPassphrase.checked);
  });

  test('ClickingLinkDoesNotChangeRadioValue', function() {
    assertFalse(encryptionRadioGroup.disabled);
    assertEquals(encryptWithPassphrase.getAttribute('aria-disabled'), 'false');
    assertFalse(encryptWithPassphrase.checked);

    const link =
        encryptWithPassphrase.querySelector<HTMLAnchorElement>('a[href]');
    assertTrue(!!link);

    // Suppress opening a new tab, since then the test will continue running
    // on a background tab (which has throttled timers) and will timeout.
    link.target = '';
    link.href = '#';
    // Prevent the link from triggering a page navigation when tapped.
    // Breaks the test in Vulcanized mode.
    link.addEventListener('click', function(e) {
      e.preventDefault();
    });

    link.click();

    assertFalse(encryptWithPassphrase.checked);
  });

  test('SaveButtonDisabledWhenPassphraseOrConfirmationEmpty', async () => {
    encryptWithPassphrase.click();
    await eventToPromise('selected-changed', encryptionRadioGroup);

    assertTrue(
        !!encryptionElement.shadowRoot!.querySelector('#create-password-box'));
    const saveNewPassphrase =
        encryptionElement.shadowRoot!.querySelector<CrButtonElement>(
            '#saveNewPassphrase')!;
    const passphraseInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseInput')!;
    const passphraseConfirmationInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseConfirmationInput')!;

    passphraseInput.value = '';
    passphraseConfirmationInput.value = '';
    await Promise.all([
      passphraseInput.updateComplete,
      passphraseConfirmationInput.updateComplete,
    ]);
    assertTrue(saveNewPassphrase.disabled);

    passphraseInput.value = 'foo';
    passphraseConfirmationInput.value = '';
    await Promise.all([
      passphraseInput.updateComplete,
      passphraseConfirmationInput.updateComplete,
    ]);
    assertTrue(saveNewPassphrase.disabled);

    passphraseInput.value = 'foo';
    passphraseConfirmationInput.value = 'bar';
    await Promise.all([
      passphraseInput.updateComplete,
      passphraseConfirmationInput.updateComplete,
    ]);
    assertFalse(saveNewPassphrase.disabled);
  });

  test('CreatingPassphraseMismatchedPassphrase', async () => {
    encryptWithPassphrase.click();
    await eventToPromise('selected-changed', encryptionRadioGroup);

    assertTrue(
        !!encryptionElement.shadowRoot!.querySelector('#create-password-box'));
    const saveNewPassphrase =
        encryptionElement.shadowRoot!.querySelector<CrButtonElement>(
            '#saveNewPassphrase');
    assertTrue(!!saveNewPassphrase);

    const passphraseInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseInput')!;
    const passphraseConfirmationInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseConfirmationInput')!;
    passphraseInput.value = 'foo';
    passphraseConfirmationInput.value = 'bar';
    await Promise.all([
      passphraseInput.updateComplete,
      passphraseConfirmationInput.updateComplete,
    ]);

    saveNewPassphrase.click();
    flush();

    assertFalse(passphraseInput.invalid);
    assertTrue(passphraseConfirmationInput.invalid);
  });

  test('CreatingPassphraseValidPassphrase', async function() {
    encryptWithPassphrase.click();
    await eventToPromise('selected-changed', encryptionRadioGroup);

    assertTrue(
        !!encryptionElement.shadowRoot!.querySelector('#create-password-box'));
    const saveNewPassphrase =
        encryptionElement.shadowRoot!.querySelector<CrButtonElement>(
            '#saveNewPassphrase');
    assertTrue(!!saveNewPassphrase);

    const passphraseInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseInput')!;
    const passphraseConfirmationInput =
        encryptionElement.shadowRoot!.querySelector<CrInputElement>(
            '#passphraseConfirmationInput')!;
    passphraseInput.value = 'foo';
    passphraseConfirmationInput.value = 'foo';
    testSyncBrowserProxy.encryptionPassphraseSuccess = true;
    await Promise.all([
      passphraseInput.updateComplete,
      passphraseConfirmationInput.updateComplete,
    ]);
    saveNewPassphrase.click();

    const passphrase =
        await testSyncBrowserProxy.whenCalled('setEncryptionPassphrase');

    assertEquals('foo', passphrase);

    // Fake backend response.
    const newPrefs = getSyncAllPrefs();
    newPrefs.encryptAllData = true;
    encryptionElement.syncPrefs = newPrefs;
    flush();

    await waitBeforeNextRender(encryptionElement);
    // Need to re-retrieve this, as a different show passphrase radio
    // button is shown for custom passphrase users.
    encryptWithPassphrase = encryptionElement.shadowRoot!.querySelector(
        'cr-radio-button[name="encrypt-with-passphrase"]')!;

    // Assert that the radio boxes are disabled after encryption enabled.
    assertTrue(encryptionRadioGroup.disabled);
    assertEquals(-1, encryptWithGoogle.$.button.tabIndex);
    assertEquals(-1, encryptWithPassphrase.$.button.tabIndex);
  });

  test('PassphraseEncryptionOptionsEnabledWhenAllowed', async () => {
    const prefs = getSyncAllPrefs();
    prefs.customPassphraseAllowed = true;
    encryptionElement.syncPrefs = prefs;
    testSyncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();

    assertFalse(encryptionRadioGroup.disabled);
    assertEquals(encryptWithGoogle.getAttribute('aria-disabled'), 'false');
    assertEquals(encryptWithPassphrase.getAttribute('aria-disabled'), 'false');
  });

  // customPassphraseAllowed is usually false only for supervised users, but
  // it's better to be check this case.
  test('PassphraseEncryptionOptionsDisabledWhenNotAllowed', async () => {
    const prefs = getSyncAllPrefs();
    prefs.customPassphraseAllowed = false;
    encryptionElement.syncPrefs = prefs;
    testSyncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      supervisedUser: false,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();

    assertTrue(encryptionRadioGroup.disabled);
    assertEquals(encryptWithGoogle.getAttribute('aria-disabled'), 'true');
    assertEquals(encryptWithPassphrase.getAttribute('aria-disabled'), 'true');

    // Because sync is required for supervision, passphrases should remain
    // disabled.
    test(
        'PassphraseEncryptionOptionsAlwaysDisabledForSupervisedUser',
        async () => {
          const prefs = getSyncAllPrefs();
          prefs.customPassphraseAllowed = false;
          encryptionElement.syncPrefs = prefs;
          testSyncBrowserProxy.testSyncStatus = {
            signedInState: SignedInState.SIGNED_IN,
            supervisedUser: true,
            statusAction: StatusAction.NO_ACTION,
          };
          await microtasksFinished();

          assertTrue(encryptionRadioGroup.disabled);
          assertEquals(encryptWithGoogle.getAttribute('aria-disabled'), 'true');
          assertEquals(
              encryptWithPassphrase.getAttribute('aria-disabled'), 'true');

          // This never happens in practice, but just to be safe we are
          // expecting the options to be disabled if full data encryption were
          // allowed for supervised users as well.
          prefs.customPassphraseAllowed = true;
          encryptionElement.syncPrefs = prefs;
          await microtasksFinished();
          assertTrue(encryptionRadioGroup.disabled);
          assertEquals(encryptWithGoogle.getAttribute('aria-disabled'), 'true');
          assertEquals(
              encryptWithPassphrase.getAttribute('aria-disabled'), 'true');
        });
  });
});
