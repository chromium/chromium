// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PasspointRemoveDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<passpoint-remove-dialog>', () => {
  let removeDialog: PasspointRemoveDialogElement;

  async function init(isPasspointSettingsEnabled: boolean) {
    loadTimeData.overrideValues({
      isPasspointSettingsEnabled,
    });
    removeDialog = document.createElement('passpoint-remove-dialog');
    assertTrue(!!removeDialog);
    document.body.appendChild(removeDialog);
    await flush();
  }

  function getButton(buttonId: string): CrButtonElement {
    const button =
        removeDialog.shadowRoot!.querySelector<CrButtonElement>(`#${buttonId}`);
    assertTrue(!!button);
    return button;
  }

  test('Network and subscription removal', async () => {
    // When "Passpoint settings flag" is disabled the dialog has to show two
    // messages.
    await init(false);

    const desc =
        removeDialog.shadowRoot!.querySelector<HTMLDivElement>('#description');
    assertTrue(!!desc);
    assertFalse(desc.hidden);

    const info =
        removeDialog.shadowRoot!.querySelector<HTMLSpanElement>('#information');
    assertTrue(!!info);

    // Confirm button shows "Confirm".
    const button = getButton('confirmButton');
    assertEquals(loadTimeData.getString('confirm'), button.textContent!.trim());
  });

  test('Go to subscription page', async () => {
    // When "Passpoint settings flag" is enabled the dialog has to show only the
    // description message.
    await init(true);

    const desc =
        removeDialog.shadowRoot!.querySelector<HTMLDivElement>('#description');
    assertTrue(!!desc);
    assertTrue(desc.hidden);

    const info =
        removeDialog.shadowRoot!.querySelector<HTMLSpanElement>('#information');
    assertTrue(!!info);

    // Confirm button shows "Go to subscription".
    const button = getButton('confirmButton');
    assertEquals(
        loadTimeData.getString(
            'networkSectionPasspointGoToSubscriptionButtonLabel'),
        button.textContent!.trim());
  });

  test('Cancel the dialog', async () => {
    await init(false);

    const button = getButton('cancelButton');
    button.click();
    await flush();

    const dialog =
        removeDialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Confirm the dialog and get the event', async () => {
    await init(false);

    const button = getButton('confirmButton');
    const confirmPromise = eventToPromise('confirm', window);
    button.click();
    await confirmPromise;
  });
});
