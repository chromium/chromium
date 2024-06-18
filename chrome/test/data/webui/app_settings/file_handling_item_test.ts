// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for AppManagementFileHandlingItem. */
import 'chrome://app-settings/file_handling_item.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';

import type {FileHandlingItemElement} from 'chrome://app-settings/file_handling_item.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTestApp, TestAppManagementBrowserProxy} from './app_management_test_support.js';

suite('AppManagementFileHandlingItemTest', function() {
  let fileHandlingItem: FileHandlingItemElement;
  let testProxy: TestAppManagementBrowserProxy;
  let app: App;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    app = createTestApp('app');
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
    await microtasksFinished();
  });

  test('File Handling overflow', async function() {
    // No overflow link because it's not in `userVisibleTypes`.
    const typeList = fileHandlingItem.shadowRoot!.querySelector('#type-list')!;
    assertTrue(!!typeList);
    let link = typeList.shadowRoot!.querySelector<HTMLElement>('a');
    assertTrue(!link);

    // Overflow link present.
    const app2 = createTestApp('app');
    app2.fileHandlingState!.userVisibleTypesLabel =
        'TXT, CSV, MD, DOC (<a href="#">and 1 more</a>)';
    fileHandlingItem.app = app2;
    await microtasksFinished();
    link = typeList.shadowRoot!.querySelector<HTMLElement>('a');
    assertTrue(!!link);

    // Dialog starts hidden.
    let dialog =
        fileHandlingItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertTrue(!dialog);
    const originalUrl = location.href;
    link.click();
    await microtasksFinished();

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
    const app2 = createTestApp('app');
    app2.fileHandlingState!.learnMoreUrl = null;
    fileHandlingItem.app = app2;
    await microtasksFinished();
    link = learnMore.shadowRoot!.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!link);
    assertEquals(link.getAttribute('href'), '#');

    link.click();
    await testProxy.handler.whenCalled('showDefaultAppAssociationsUi');
  });
});
