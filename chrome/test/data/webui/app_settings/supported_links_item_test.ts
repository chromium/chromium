// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for SupportedLinksItemElement. */
import 'chrome://app-settings/supported_links_item.js';

import type {SupportedLinksItemElement} from 'chrome://app-settings/supported_links_item.js';
import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import type {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import type {AppConfig} from './app_management_test_support.js';
import {createTestApp, TestAppManagementBrowserProxy} from './app_management_test_support.js';

suite('SupportedLinksItemElement', function() {
  let supportedLinksItem: SupportedLinksItemElement;
  let testProxy: TestAppManagementBrowserProxy;
  let apps: AppMap;

  setup(function() {
    apps = {};
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestAppManagementBrowserProxy();
    BrowserProxy.setInstance(testProxy);

    loadTimeData.resetForTesting({
      cancel: 'Cancel',
      close: 'Close',
      appManagementIntentSettingsDialogTitle: 'Supported Links',
      appManagementIntentSettingsTitle: '<a href="#">Supported Links</a>',
      appManagementIntentOverlapDialogTitle: 'Change as preferred app',
      appManagementIntentOverlapChangeButton: 'Change',
      appManagementIntentSharingOpenBrowserLabel: 'Open in Chrome browser',
      appManagementIntentSharingOpenAppLabel: 'Open in App',
      appManagementIntentSharingTabExplanation:
          'App is set to open in a new browser tab, supported links will also open in the browser.',
    });
  });

  async function setUpSupportedLinksComponent(
      id: string, optConfig?: AppConfig): Promise<App> {
    const app = createTestApp(id, optConfig);
    apps[app.id] = app;
    supportedLinksItem =
        document.createElement('app-management-supported-links-item');
    supportedLinksItem.app = app;
    supportedLinksItem.apps = apps;
    document.body.appendChild(supportedLinksItem);
    await microtasksFinished();
    return app;
  }

  test('No supported links', async () => {
    const appOptions = {
      type: AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: [],  // Explicitly empty.
    };
    await setUpSupportedLinksComponent('app1', appOptions);
    assertFalse(isVisible(supportedLinksItem));
  });

  test('Window/Tab mode', async () => {
    const appOptions = {
      type: AppType.kWeb,
      isPreferredApp: true,
      windowMode: WindowMode.kBrowser,
      supportedLinks: ['google.com'],
    };

    await setUpSupportedLinksComponent('app1', appOptions);
    assertTrue(!!supportedLinksItem.shadowRoot!.querySelector(
        '#disabledExplanationText'));

    const radioGroup =
        supportedLinksItem.shadowRoot!.querySelector<CrRadioGroupElement>(
            '#radioGroup');
    assertTrue(!!radioGroup);
    assertTrue(!!radioGroup.disabled);
  });

  test('can open and close supported links list dialog', async () => {
    const supportedLink = 'google.com';
    const appOptions = {
      type: AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: [supportedLink],
    };

    await setUpSupportedLinksComponent('app1', appOptions);
    let supportedLinksDialog =
        supportedLinksItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertNull(supportedLinksDialog);

    // Open dialog.
    const heading = supportedLinksItem.shadowRoot!.querySelector('#heading');
    assertTrue(!!heading);
    const link = heading.shadowRoot!.querySelector('a');
    assertTrue(!!link);
    link.click();
    await microtasksFinished();

    supportedLinksDialog =
        supportedLinksItem.shadowRoot!.querySelector<HTMLElement>('#dialog');
    assertTrue(!!supportedLinksDialog);
    const innerDialog =
        supportedLinksDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!innerDialog);
    assertTrue(innerDialog.open);

    // Confirm google.com shows up.
    const list = supportedLinksDialog.shadowRoot!.querySelector('#list');
    assertTrue(!!list);
    const item = list.getElementsByClassName('list-item')[0] as HTMLElement;
    assertTrue(!!item);
    assertEquals(supportedLink, item.innerText);

    // Close dialog.
    const closeButton =
        innerDialog.shadowRoot!.querySelector<HTMLButtonElement>('#close');
    assertTrue(!!closeButton);
    closeButton.click();
    await microtasksFinished();

    // Wait for the stamped dialog to be destroyed.
    supportedLinksDialog =
        supportedLinksItem.shadowRoot!.querySelector('#dialog');
    assertNull(supportedLinksDialog);
  });
});
