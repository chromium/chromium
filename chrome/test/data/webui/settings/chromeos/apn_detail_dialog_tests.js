// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/apn_detail_dialog.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertTrue} from '../../../chai_assert.js';

suite('ApnDetailDialog', function() {
  /** @type {ApnDetalDialog} */
  let apnDetailDialog = null;

  async function toggleAdvancedSettings() {
    const advancedSettingsBtn =
        apnDetailDialog.shadowRoot.querySelector('#advancedSettingsBtn');
    assertTrue(!!advancedSettingsBtn);
    advancedSettingsBtn.click();
  }

  setup(function() {
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    apnDetailDialog = document.createElement('apn-detail-dialog');
    document.body.appendChild(apnDetailDialog);

    return flushTasks();
  });

  teardown(function() {
    return flushTasks().then(() => {
      apnDetailDialog.remove();
      apnDetailDialog = null;
    });
  });

  test('Element contains dialog', function() {
    const dialog = apnDetailDialog.shadowRoot.querySelector('cr-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    // Confirm that the dialog has the add apn title.
    assertEquals(
        apnDetailDialog.i18n('apnDetailAddApnDialogTitle'),
        apnDetailDialog.shadowRoot.querySelector('#apnDetailDialogTitle')
            .innerText);
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#apnInput'));
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#usernameInput'));
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#passwordInput'));

    assertTrue(!!apnDetailDialog.shadowRoot.querySelector(
        '#authenticationTypeSelection'));
    assertTrue(
        !!apnDetailDialog.shadowRoot.querySelector('#apnDefaultTypeCheckbox'));
    assertTrue(
        !!apnDetailDialog.shadowRoot.querySelector('#apnAttachTypeCheckbox'));
    assertTrue(!!apnDetailDialog.shadowRoot.querySelector('#ipTypeSelection'));

  });

  test('Clicking the cancel button fires the close event', async function() {
    const closeEventPromise = eventToPromise('close', window);
    const cancelBtn =
        apnDetailDialog.shadowRoot.querySelector('#apnDetailCancelBtn');
    assertTrue(!!cancelBtn);

    cancelBtn.click();
    await closeEventPromise;
    assertFalse(!!apnDetailDialog.open);
  });

  test(
      'Clicking on the advanced settings button expands/collapses section',
      function() {
        const isAdvancedSettingShowing = () =>
            apnDetailDialog.shadowRoot.querySelector('iron-collapse').opened;
        assertFalse(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        assertTrue(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        assertFalse(!!isAdvancedSettingShowing());
        toggleAdvancedSettings();
        const assertOptions = (expectedTextArray, optionNodes) => {
          for (const [idx, expectedText] of expectedTextArray.entries()) {
            assertTrue(!!optionNodes[idx]);
            assertTrue(!!optionNodes[idx].text);
            assertEquals(expectedText, optionNodes[idx].text);
          }
        };
        const authTypeDropDown =
            apnDetailDialog.shadowRoot.querySelector('#authTypeDropDown');
        assertTrue(!!authTypeDropDown);
        const authTypeOptionNodes = authTypeDropDown.querySelectorAll('option');
        assertEquals(3, authTypeOptionNodes.length);
        // Note: We are also checking that the items appear in a certain order.
        assertOptions(
            [
              apnDetailDialog.i18n('apnDetailTypeAuto'),
              apnDetailDialog.i18n('apnDetailAuthTypePAP'),
              apnDetailDialog.i18n('apnDetailAuthTypeCHAP'),
            ],
            authTypeOptionNodes);

        const ipTypeDropDown =
            apnDetailDialog.shadowRoot.querySelector('#ipTypeDropDown');
        assertTrue(!!ipTypeDropDown);
        const ipTypeOptionNodes = ipTypeDropDown.querySelectorAll('option');
        assertEquals(4, ipTypeOptionNodes.length);

        assertOptions(
            [
              apnDetailDialog.i18n('apnDetailTypeAuto'),
              apnDetailDialog.i18n('apnDetailIPTypeIPV4'),
              apnDetailDialog.i18n('apnDetailIPTypeIPV6'),
              apnDetailDialog.i18n('apnDetailIPTypeIPV4_IPV6'),
            ],
            ipTypeOptionNodes);
      });
});
