// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {InlineCueMenuPageElement, SettingsToggleButtonElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';
// clang-format on

suite('InlineCueMenuPage', function() {
  let browserProxy: TestSiteSettingsBrowserProxy;
  let page: InlineCueMenuPageElement;

  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(function() {
    page.remove();
  });

  async function initPage(prefs = createSiteSettingsPrefs([], [])) {
    browserProxy.setPrefs(prefs);
    page = document.createElement('settings-inline-cue-menu-page');
    document.body.appendChild(page);
    await flushTasks();
  }

  test('MainToggle', async function() {
    await initPage();
    const mainToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mainToggle');
    assertTrue(!!mainToggle);
    assertTrue(mainToggle.checked);

    mainToggle.click();
    const args = await browserProxy.whenCalled('setDefaultValueForContentType');
    assertEquals(ContentSettingsTypes.INLINE_CUE_MENU, args[0]);
    assertEquals(ContentSetting.BLOCK, args[1]);
  });

  test('BlockedSitesListUpdate', async function() {
    await initPage();
    const noSitesLabel =
        page.shadowRoot!.querySelector<HTMLElement>('.list-frame .secondary');
    assertTrue(!!noSitesLabel);
    assertTrue(isVisible(noSitesLabel));

    const siteListItems = page.shadowRoot!.querySelectorAll<HTMLElement>(
        '.list-item[role=listitem]');
    assertEquals(0, siteListItems.length);
  });

  test('BlockedSitesListRender', async function() {
    const prefs = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.INLINE_CUE_MENU,
          [
            createRawSiteException('https://foo.com', {
              setting: ContentSetting.BLOCK,
              displayName: 'foo.com',
            }),
          ]),
    ]);
    await initPage(prefs);

    const siteListItems = page.shadowRoot!.querySelectorAll<HTMLElement>(
        '.list-item[role=listitem]');
    assertEquals(1, siteListItems.length);
    assertTrue(siteListItems[0]!.textContent.includes('foo.com'));
  });

  test('BlockedSitesListDelete', async function() {
    const prefs = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.INLINE_CUE_MENU,
          [
            createRawSiteException('https://foo.com', {
              setting: ContentSetting.BLOCK,
              displayName: 'foo.com',
            }),
            createRawSiteException('https://bar.com', {
              setting: ContentSetting.BLOCK,
              displayName: 'bar.com',
            }),
          ]),
    ]);
    await initPage(prefs);

    const siteListItems = page.shadowRoot!.querySelectorAll<HTMLElement>(
        '.list-item[role=listitem]');
    assertEquals(2, siteListItems.length);

    const deleteButton = siteListItems[0]!.querySelector<HTMLElement>(
        'cr-icon-button[iron-icon="cr:delete"]');
    assertTrue(!!deleteButton);
    deleteButton.click();

    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals('https://foo.com', args[0]);
    assertEquals('https://foo.com', args[1]);
    assertEquals(ContentSettingsTypes.INLINE_CUE_MENU, args[2]);
    assertFalse(args[3]);
  });

  test('AddSiteDialog', async function() {
    await initPage();
    assertFalse(!!page.shadowRoot!.querySelector('add-site-dialog'));

    const addSiteButton =
        page.shadowRoot!.querySelector<HTMLElement>('#addSite');
    assertTrue(!!addSiteButton);
    addSiteButton.click();
    await flushTasks();

    const addSiteDialog = page.shadowRoot!.querySelector('add-site-dialog');
    assertTrue(!!addSiteDialog);
  });
});
