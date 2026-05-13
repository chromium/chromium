// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsAiModeSearchPageElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('AiModeSearchSubpage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsAiModeSearchPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-mode-search-page');
    subpage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  test('shareTabsEveryThreadToggle', async () => {
    await createPage();

    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    subpage.set('prefs', {
      contextual_tasks: {
        share_open_tabs_every_thread: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        site_exclusions: {
          type: chrome.settingsPrivate.PrefType.DICTIONARY,
          value: {},
        },
      },
    });
    assertFalse(toggle.checked);

    // Click toggle
    toggle.click();
    assertTrue(
        subpage
            .getPref<boolean>('contextual_tasks.share_open_tabs_every_thread')
            .value);
    assertTrue(toggle.checked);

    // Click again
    toggle.click();
    assertFalse(
        subpage
            .getPref<boolean>('contextual_tasks.share_open_tabs_every_thread')
            .value);
    assertFalse(toggle.checked);
  });

  test('learnMoreLinkRow', async function() {
    await createPage();

    const linkout = subpage.shadowRoot!.querySelector('cr-link-row');
    assertTrue(!!linkout);

    linkout.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('https://support.google.com/chrome?p=ai_mode_search', url);
    openWindowProxy.reset();
  });

  test('learnMoreLink', async () => {
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        'https://support.google.com/chrome?p=ai_mode_search',
        learnMoreLink.href);
  });

  test('siteExclusionsPref', async () => {
    await createPage();

    subpage.set('prefs', {
      contextual_tasks: {
        share_open_tabs_every_thread: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        site_exclusions: {
          type: chrome.settingsPrivate.PrefType.DICTIONARY,
          value: {},
        },
      },
    });
    let exclusions = subpage.getSiteExclusions();
    assertEquals(0, Object.keys(exclusions).length);

    const timeAddedMs = Date.now();
    subpage.addSiteExclusion('example.com', timeAddedMs);

    exclusions = subpage.getSiteExclusions();
    assertEquals(1, Object.keys(exclusions).length);
    assertEquals(timeAddedMs, exclusions['example.com']);
  });

  test('siteExclusionsUi', async () => {
    await createPage();

    subpage.set('prefs', {
      contextual_tasks: {
        share_open_tabs_every_thread: {
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        site_exclusions: {
          type: chrome.settingsPrivate.PrefType.DICTIONARY,
          value: {},
        },
      },
    });

    // Check initial state
    const siteList = subpage.shadowRoot!.querySelector('#siteList');
    assertTrue(!!siteList);
    let items = siteList.querySelectorAll('.list-item');
    assertEquals(0, items.length);

    // Open add dialog
    const addButton =
        subpage.shadowRoot!.querySelector<HTMLElement>('#addSiteButton');
    assertTrue(!!addButton);
    addButton.click();
    await flushTasks();

    const addDialog = subpage.shadowRoot!.querySelector('ai-site-add-dialog');
    assertTrue(!!addDialog);

    // Type a site and submit
    const siteInput = addDialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!siteInput);
    addDialog.set('site_', 'ZeBrA.cOm');
    await flushTasks();

    const dialogAddButton =
        addDialog.shadowRoot!.querySelector<HTMLElement>('#add');
    assertTrue(!!dialogAddButton);
    assertFalse(dialogAddButton.hasAttribute('disabled'));
    const closePromise1 = eventToPromise('close', addDialog);
    dialogAddButton.click();
    await Promise.all([closePromise1, flushTasks()]);

    const exclusionsAfter = subpage.getSiteExclusions();
    assertEquals(
        1, Object.keys(exclusionsAfter).length,
        'getSiteExclusions should return 1 item');

    // Check list updated
    items = siteList.querySelectorAll('.list-item');
    assertEquals(1, items.length, 'DOM should have 1 item');
    assertTrue(items[0]!.textContent.includes('zebra.com'));

    // Open add dialog again
    addButton.click();
    await flushTasks();

    // Re-query the dialog because it is inside a dom-if and was recreated
    const addDialog2 = subpage.shadowRoot!.querySelector('ai-site-add-dialog');
    assertTrue(!!addDialog2);

    // Type a second site that should sort before the first one
    const siteInput2 = addDialog2.shadowRoot!.querySelector('cr-input');
    assertTrue(!!siteInput2);
    addDialog2.set('site_', 'apple.com');
    await flushTasks();

    const dialogAddButton2 =
        addDialog2.shadowRoot!.querySelector<HTMLElement>('#add');
    assertTrue(!!dialogAddButton2);
    assertFalse(dialogAddButton2.hasAttribute('disabled'));
    dialogAddButton2.click();
    await flushTasks();

    // Check list sorted
    items = siteList.querySelectorAll('.list-item');
    assertEquals(2, items.length);
    assertTrue(items[0]!.textContent.includes('apple.com'));
    assertTrue(items[1]!.textContent.includes('zebra.com'));

    // Edit site
    const menuButton = items[0]!.querySelector('cr-icon-button');
    assertTrue(!!menuButton);
    menuButton.click();
    await flushTasks();

    const actionMenu = subpage.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const editButton = actionMenu.querySelector<HTMLElement>('#edit');
    assertTrue(!!editButton);
    editButton.click();
    await flushTasks();

    // Verify it opens the dialog pre-populated
    const editDialog = subpage.shadowRoot!.querySelector('ai-site-add-dialog');
    assertTrue(!!editDialog);
    const editInput = editDialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!editInput);
    assertEquals('apple.com', editInput.value);

    // Change value and save
    editDialog.set('site_', 'banana.com');
    await flushTasks();
    const saveButton =
        editDialog.shadowRoot!.querySelector<HTMLElement>('#add');
    assertTrue(!!saveButton);
    const closePromise3 = eventToPromise('close', editDialog);
    saveButton.click();
    await Promise.all([closePromise3, flushTasks()]);

    // Check list sorted
    items = siteList.querySelectorAll('.list-item');
    assertEquals(2, items.length);
    assertTrue(items[0]!.textContent.includes('banana.com'));
    assertTrue(items[1]!.textContent.includes('zebra.com'));

    // Remove site
    const removeMenuButton = items[0]!.querySelector('cr-icon-button');
    assertTrue(!!removeMenuButton);
    removeMenuButton.click();
    await flushTasks();

    assertTrue(actionMenu.open);
    const deleteButton = actionMenu.querySelector<HTMLElement>('#delete');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    items = siteList.querySelectorAll('.list-item');
    assertEquals(1, items.length);
    assertTrue(items[0]!.textContent.includes('zebra.com'));
  });
});
