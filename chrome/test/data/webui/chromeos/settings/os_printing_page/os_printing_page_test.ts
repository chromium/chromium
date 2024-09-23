// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsPrintingPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<os-settings-printing-page>', function() {
  let printingPage: OsSettingsPrintingPageElement;

  setup(async function() {
    printingPage = document.createElement('os-settings-printing-page');
    assert(printingPage);
    document.body.appendChild(printingPage);
    await flushTasks();
  });

  teardown(function() {
    printingPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Printing settings card is visible', async () => {
    const printingSettingsCard =
        printingPage.shadowRoot!.querySelector('printing-settings-card');
    assertTrue(isVisible(printingSettingsCard));
  });
});
