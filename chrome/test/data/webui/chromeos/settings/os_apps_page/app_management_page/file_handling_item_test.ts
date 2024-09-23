// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for AppManagementFileHandlingItem. */
import 'chrome://os-settings/os_settings.js';

import {AppManagementFileHandlingItemElement} from 'chrome://os-settings/os_settings.js';
import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createApp, setupFakeHandler} from '../../app_management/test_util.js';
import {clearBody} from '../../utils.js';

suite('AppManagementFileHandlingItemTest', function() {
  let fileHandlingItem: AppManagementFileHandlingItemElement;
  let app: App;

  const fileHandlingState = {
    enabled: false,
    isManaged: false,
    userVisibleTypes: 'TXT',
    userVisibleTypesLabel: 'Supported type: TXT',
    learnMoreUrl: {url: 'https://google.com/'},
  };

  setup(async function() {
    clearBody();
    setupFakeHandler();
    app = createApp('app', {fileHandlingState});

    loadTimeData.resetForTesting({
      close: 'Close',
      appManagementFileHandlingHeader: 'Enable File Handling',
      fileHandlingOverflowDialogTitle: 'Overflow dialog',
      fileHandlingSetDefaults: 'Learn more <a href="#">here</a>',
    });

    fileHandlingItem =
        document.createElement('app-management-file-handling-item');
    fileHandlingItem.app = app;
    document.body.appendChild(fileHandlingItem);
    await waitAfterNextRender(fileHandlingItem);
  });

  test('File Handling overflow', async function() {
    // No overflow link because it's not in `userVisibleTypes`.
    const typeList = fileHandlingItem.shadowRoot!.querySelector('#type-list')!;
    assertTrue(!!typeList);
    let link = typeList.shadowRoot!.querySelector<HTMLElement>('a');
    assertTrue(!link);

    // Overflow link present.
    const app2 = createApp('app', {fileHandlingState});
    app2.fileHandlingState!.userVisibleTypesLabel =
        'TXT, CSV, MD, DOC (<a href="#">and 1 more</a>)';
    fileHandlingItem.app = app2;
    await flushTasks();
    link = typeList.shadowRoot!.querySelector<HTMLElement>('a');
    assertTrue(!!link);

    // Dialog starts hidden.
    let dialog =
        fileHandlingItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertTrue(!dialog);
    const originalUrl = location.href;
    link.click();
    flush();

    // Clicking the link doesn't change the URL, and does open the dialog.
    assertEquals(originalUrl, location.href);
    dialog = fileHandlingItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertTrue(!!dialog);
  });

  test('File Handling learn more', async function() {
    const learnMore =
        fileHandlingItem.shadowRoot!.querySelector<HTMLElement>('#learn-more')!;
    assertTrue(!!learnMore);
    const link = learnMore.shadowRoot!.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!link);
    assertEquals(link.href, app.fileHandlingState!.learnMoreUrl!.url);
  });
});
