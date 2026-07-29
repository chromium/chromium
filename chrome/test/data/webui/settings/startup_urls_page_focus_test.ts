// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsStartupUrlsPageElement, StartupPageInfo} from 'chrome://settings/settings.js';
import {StartupUrlsPageBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestStartupUrlsPageBrowserProxy} from './test_startup_urls_page_browser_proxy.js';

suite('StartupUrlsPageFocus', function() {
  let page: SettingsStartupUrlsPageElement;
  let browserProxy: TestStartupUrlsPageBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.setInstance(browserProxy);
    page = document.createElement('settings-startup-urls-page');
    document.body.appendChild(page);
  });

  test('FocusRestorationOnDelete', async function() {
    const entry1: StartupPageInfo = {
      modelIndex: 0,
      title: 'Test page 1',
      tooltip: 'test tooltip',
      url: 'chrome://bar',
    };
    const entry2: StartupPageInfo = {
      modelIndex: 1,
      title: 'Test page 2',
      tooltip: 'test tooltip',
      url: 'chrome://foo',
    };
    const entry3: StartupPageInfo = {
      modelIndex: 2,
      title: 'Test page 3',
      tooltip: 'test tooltip',
      url: 'chrome://baz',
    };

    webUIListenerCallback('update-startup-pages', [entry1, entry2, entry3]);
    await microtasksFinished();

    let entries =
        page.shadowRoot.querySelectorAll('settings-startup-url-entry');
    assertEquals(3, entries.length);

    // Case1: Delete last item while focused.
    let entry = entries[2]!;
    entry.focus();
    let dots = entry.shadowRoot.querySelector('#dots');
    assertTrue(!!dots);
    assertEquals(dots, entry.shadowRoot.activeElement);
    webUIListenerCallback('update-startup-pages', [entry1, entry2]);
    await microtasksFinished();

    // Verify focus moved to the new last item's dots button.
    entries = page.shadowRoot.querySelectorAll('settings-startup-url-entry');
    assertEquals(2, entries.length);
    entry = entries[1]!;
    dots = entry.shadowRoot.querySelector('#dots');
    assertEquals(dots, entry.shadowRoot.activeElement);

    // Case2: Delete first item while focused.
    entry = entries[0]!;
    entry.focus();
    dots = entry.shadowRoot.querySelector('#dots');
    assertTrue(!!dots);
    assertEquals(dots, entry.shadowRoot.activeElement);
    webUIListenerCallback('update-startup-pages', [entry2]);
    await microtasksFinished();

    // Verify focus moved to the item that took its place (entry2, now at index
    // 0).
    entries = page.shadowRoot.querySelectorAll('settings-startup-url-entry');
    assertEquals(1, entries.length);
    entry = entries[0]!;
    dots = entry.shadowRoot.querySelector('#dots');
    assertEquals(dots, entry.shadowRoot.activeElement);

    // Case3: Delete last remaining item while focused.
    // Simulate deleting the last/only item.
    webUIListenerCallback('update-startup-pages', []);
    await microtasksFinished();

    // Verify focus is lost (no entry has focus).
    entries = page.shadowRoot.querySelectorAll('settings-startup-url-entry');
    assertEquals(0, entries.length);
    assertEquals(null, page.shadowRoot.activeElement);
  });
});
