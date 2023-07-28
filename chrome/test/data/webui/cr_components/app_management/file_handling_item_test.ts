// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for AppManagementFileHandlingItem. */
import 'chrome://resources/cr_components/app_management/file_handling_item.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementFileHandlingItemElement} from 'chrome://resources/cr_components/app_management/file_handling_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createTestApp, TestAppManagementBrowserProxy} from './app_management_test_support.js';

suite('AppManagementFileHandlingItemTest', function() {
  let fileHandlingItem: AppManagementFileHandlingItemElement;
  let testProxy: TestAppManagementBrowserProxy;
  let app: App;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = createTestApp();
    testProxy = new TestAppManagementBrowserProxy();
    BrowserProxy.setInstance(testProxy);

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
    const app2 = createTestApp();
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
    let link = learnMore.shadowRoot!.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!link);
    assertEquals(link.href, app.fileHandlingState!.learnMoreUrl!.url);

    // Clear the learn more url; it should now be handled by the browser proxy.
    const app2 = createTestApp();
    app2.fileHandlingState!.learnMoreUrl = undefined;
    fileHandlingItem.app = app2;
    await flushTasks();
    link = learnMore.shadowRoot!.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!link);
    assertEquals(link.getAttribute('href'), '#');

    link.click();
    await testProxy.handler.whenCalled('showDefaultAppAssociationsUi');
  });
});
