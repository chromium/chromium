// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonsSubsectionElement} from 'chrome://os-settings/lazy_load.js';
import {fakeGraphicsTabletButtonActions, fakeGraphicsTablets} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
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
  });
});
