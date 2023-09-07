// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {FkeyRowElement, SettingsDropdownMenuElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<fkey-row>', () => {
  let fkeyRow: FkeyRowElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    if (!fkeyRow) {
      return;
    }
    fkeyRow.remove();
  });

  function initializeFkeyRow() {
    fkeyRow = document.createElement(FkeyRowElement.is);
    document.body.appendChild(fkeyRow);
    return flushTasks();
  }

  function getDropdownMenuElement(): SettingsDropdownMenuElement {
    const dropdown = fkeyRow.shadowRoot!.querySelector('#keyDropdown');
    assert(dropdown);
    return dropdown as SettingsDropdownMenuElement;
  }

  test('Initialize fkey row', async () => {
    await initializeFkeyRow();
    assertTrue(!!fkeyRow.shadowRoot!.querySelector('#fkeyRow'));
    assertTrue(getDropdownMenuElement().disabled);
  });
});
