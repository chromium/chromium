// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsAiModeSearchPageElement} from 'chrome://settings/lazy_load.js';
import {OpenWindowProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'contextual_tasks.share_open_tabs_every_thread',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'contextual_tasks.site_exclusions',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
    },
    {
      key: 'contextual_tasks.smart_tab_sharing_settings',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 0,
    },
  ];
}

suite('AiModeSearchSubpage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsAiModeSearchPageElement;
  let prefService: PrefService;

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  teardown(function() {
    openWindowProxy.reset();
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-mode-search-page');
    document.body.appendChild(subpage);
    return microtasksFinished();
  }

  test('shareTabsEveryThreadToggle', async () => {
    await createPage();

    const toggle = subpage.shadowRoot.querySelector('settings-toggle-button');
    assertTrue(!!toggle);
    assertFalse(toggle.checked);

    // Click toggle
    toggle.click();
    assertTrue(
        prefService
            .getPref<boolean>('contextual_tasks.share_open_tabs_every_thread')
            .value);
    assertTrue(toggle.checked);

    // Click again
    toggle.click();
    assertFalse(
        prefService
            .getPref<boolean>('contextual_tasks.share_open_tabs_every_thread')
            .value);
    assertFalse(toggle.checked);
  });

  test('smartTabSharingNotDisabledByPolicy', async () => {
    await createPage();

    const indicator =
        subpage.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertFalse(!!indicator);

    const linkout = subpage.shadowRoot.querySelector('cr-link-row');
    assertTrue(!!linkout);
  });

  test('smartTabSharingDisabledByPolicy', async () => {
    await createPage();
    await prefService.setPrefValue(
        'contextual_tasks.smart_tab_sharing_settings', 1);
    await microtasksFinished();

    const indicator =
        subpage.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);

    const toggle = subpage.shadowRoot.querySelector('settings-toggle-button');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);

    const linkout = subpage.shadowRoot.querySelector('cr-link-row');
    assertFalse(!!linkout);
  });

  test('learnMoreLinkRow', async function() {
    await createPage();

    const linkout = subpage.shadowRoot.querySelector('cr-link-row');
    assertTrue(!!linkout);

    linkout.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('https://support.google.com/chrome?p=ai_mode_search', url);
    openWindowProxy.reset();
  });

  test('learnMoreLink', async () => {
    await createPage();

    const learnMoreLink = subpage.shadowRoot.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        'https://support.google.com/chrome?p=ai_mode_search',
        learnMoreLink.href);
  });

  test('siteExclusionsPref', async () => {
    await createPage();

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

    // Check initial state
    const siteList = subpage.shadowRoot.querySelector('#siteList');
    assertTrue(!!siteList);
    let items = siteList.querySelectorAll('.list-item');
    assertEquals(0, items.length);

    // Open add dialog
    const addButton =
        subpage.shadowRoot.querySelector<HTMLElement>('#addSiteButton');
    assertTrue(!!addButton);
    addButton.click();
    await microtasksFinished();

    const addDialog = subpage.shadowRoot.querySelector('ai-site-add-dialog');
    assertTrue(!!addDialog);

    // Type a site and submit
    const siteInput = addDialog.shadowRoot.querySelector('cr-input');
    assertTrue(!!siteInput);
    siteInput.value = 'ZeBrA.cOm';
    await microtasksFinished();

    const dialogAddButton =
        addDialog.shadowRoot.querySelector<HTMLElement>('#add');
    assertTrue(!!dialogAddButton);
    assertFalse(dialogAddButton.hasAttribute('disabled'));
    const closePromise1 = eventToPromise('close', addDialog);
    dialogAddButton.click();
    await Promise.all([closePromise1, microtasksFinished()]);

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
    await microtasksFinished();

    // Re-query the dialog because it is conditionally rendered and was
    // recreated
    const addDialog2 = subpage.shadowRoot.querySelector('ai-site-add-dialog');
    assertTrue(!!addDialog2);

    // Type a second site that should sort before the first one
    const siteInput2 = addDialog2.shadowRoot.querySelector('cr-input');
    assertTrue(!!siteInput2);
    siteInput2.value = 'apple.com';
    await microtasksFinished();

    const dialogAddButton2 =
        addDialog2.shadowRoot.querySelector<HTMLElement>('#add');
    assertTrue(!!dialogAddButton2);
    assertFalse(dialogAddButton2.hasAttribute('disabled'));
    dialogAddButton2.click();
    await microtasksFinished();

    // Check list sorted
    items = siteList.querySelectorAll('.list-item');
    assertEquals(2, items.length);
    assertTrue(items[0]!.textContent.includes('apple.com'));
    assertTrue(items[1]!.textContent.includes('zebra.com'));

    // Edit site
    const menuButton = items[0]!.querySelector('cr-icon-button');
    assertTrue(!!menuButton);
    menuButton.click();
    await microtasksFinished();

    const actionMenu = subpage.shadowRoot.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const editButton = actionMenu.querySelector<HTMLElement>('#edit');
    assertTrue(!!editButton);
    editButton.click();
    await microtasksFinished();

    // Verify it opens the dialog pre-populated
    const editDialog = subpage.shadowRoot.querySelector('ai-site-add-dialog');
    assertTrue(!!editDialog);
    const editInput = editDialog.shadowRoot.querySelector('cr-input');
    assertTrue(!!editInput);
    assertEquals('apple.com', editInput.value);

    // Change value and save
    editInput.value = 'banana.com';
    await microtasksFinished();
    const saveButton = editDialog.shadowRoot.querySelector<HTMLElement>('#add');
    assertTrue(!!saveButton);
    const closePromise3 = eventToPromise('close', editDialog);
    saveButton.click();
    await Promise.all([closePromise3, microtasksFinished()]);

    // Check list sorted
    items = siteList.querySelectorAll('.list-item');
    assertEquals(2, items.length);
    assertTrue(items[0]!.textContent.includes('banana.com'));
    assertTrue(items[1]!.textContent.includes('zebra.com'));

    // Remove site
    const removeMenuButton = items[0]!.querySelector('cr-icon-button');
    assertTrue(!!removeMenuButton);
    removeMenuButton.click();
    await microtasksFinished();

    assertTrue(actionMenu.open);
    const deleteButton = actionMenu.querySelector<HTMLElement>('#delete');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await microtasksFinished();

    items = siteList.querySelectorAll('.list-item');
    assertEquals(1, items.length);
    assertTrue(items[0]!.textContent.includes('zebra.com'));
  });
});
