// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonsSubsectionElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets} from 'chrome://os-settings/os_settings.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-buttons-subsection>', () => {
  let customizeButtonsSubsection: CustomizeButtonsSubsectionElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!customizeButtonsSubsection) {
      return;
    }
    customizeButtonsSubsection.remove();
    await flushTasks();
  });

  async function initializeCustomizeButtonsSubsection() {
    customizeButtonsSubsection =
        document.createElement(CustomizeButtonsSubsectionElement.is);
    customizeButtonsSubsection.set(
        'actionList', fakeGraphicsTabletButtonActions);
    customizeButtonsSubsection.set(
        'buttonRemappingList',
        fakeGraphicsTablets[0]!.settings!.penButtonRemappings);
    document.body.appendChild(customizeButtonsSubsection);
    return await flushTasks();
  }

  test('Initialize customize buttons subsection', async () => {
    await initializeCustomizeButtonsSubsection();
    assertTrue(!!customizeButtonsSubsection);
    assertTrue(!!customizeButtonsSubsection.get('buttonRemappingList'));

    // Verify that renaming dialog will pop out when setting
    // shouldShowRenamingDialog_ to true.
    customizeButtonsSubsection.set('shouldShowRenamingDialog_', true);
    customizeButtonsSubsection.set(
        'selectedButton_',
        fakeGraphicsTablets[0]!.settings!.penButtonRemappings[0]);
    await flushTasks();
    assertTrue(!!customizeButtonsSubsection.shadowRoot!.querySelector(
        '#renamingDialog'));

    // Verify that renaming dialog will disappear after clicking save button.
    const saveButton: CrButtonElement|null =
        customizeButtonsSubsection.shadowRoot!.querySelector('#saveButton');
    assertTrue(!!saveButton);
    saveButton.click();
    await flushTasks();
    assertFalse(customizeButtonsSubsection.get('shouldShowRenamingDialog_'));
    assertFalse(!!customizeButtonsSubsection.shadowRoot!.querySelector(
        '#renamingDialog'));
  });
});
