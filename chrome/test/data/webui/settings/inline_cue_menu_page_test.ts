// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsInlineCueMenuPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData, PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';
// clang-format on

const EXPLAIN_PREF_KEY = 'glic.quick_answers_fre_accepted';

suite('InlineCueMenuPage', function() {
  let browserProxy: TestSiteSettingsBrowserProxy;
  let page: SettingsInlineCueMenuPageElement;

  setup(async function() {
    loadTimeData.overrideValues({
      enableQuickAnswersSelectionSuggestions: true,
      glicAssetsV2Enabled: false,
      enableGlicSelectionOverlayPromptBox: false,
    });
    const prefsBrowserProxy = new TestPrefsBrowserProxy([{
      key: EXPLAIN_PREF_KEY,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

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
    await Promise.all([
      browserProxy.whenCalled('getDefaultValueForContentType'),
      browserProxy.whenCalled('getExceptionList'),
    ]);
    await microtasksFinished();
  }

  test('MainToggle', async function() {
    await initPage();
    const mainToggle = page.$.mainToggle;
    assertTrue(mainToggle.checked);

    mainToggle.click();
    const args = await browserProxy.whenCalled('setDefaultValueForContentType');
    assertEquals(ContentSettingsTypes.INLINE_CUE_MENU, args[0]);
    assertEquals(ContentSetting.BLOCK, args[1]);
  });

  test('BlockedSitesListUpdate', async function() {
    await initPage();
    const noSitesLabel =
        page.shadowRoot.querySelector<HTMLElement>('.list-frame .secondary');
    assertTrue(!!noSitesLabel);
    assertTrue(isVisible(noSitesLabel));

    const siteListItems = page.shadowRoot.querySelectorAll<HTMLElement>(
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

    const siteListItems = page.shadowRoot.querySelectorAll<HTMLElement>(
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

    const siteListItems = page.shadowRoot.querySelectorAll<HTMLElement>(
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
    assertFalse(!!page.shadowRoot.querySelector('add-site-dialog'));

    const addSiteButton =
        page.shadowRoot.querySelector<HTMLElement>('#addSite');
    assertTrue(!!addSiteButton);
    addSiteButton.click();
    await microtasksFinished();

    const addSiteDialog = page.shadowRoot.querySelector('add-site-dialog');
    assertTrue(!!addSiteDialog);
  });

  function getExplainToggle(): SettingsToggleButtonElement|null {
    return page.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#explainToggle');
  }

  async function acceptExplainFre() {
    await PrefService.getInstance().setPrefValue(EXPLAIN_PREF_KEY, true);
    await microtasksFinished();
  }

  test('ExplainToggleGrayedOutUntilFreAccepted', async function() {
    await initPage();
    const toggle = getExplainToggle();
    assertTrue(!!toggle);
    assertFalse(toggle.checked);
    assertTrue(toggle.disabled);

    await acceptExplainFre();
    assertTrue(toggle.checked);
    assertFalse(toggle.disabled);
  });

  test('ExplainToggleTurnsOffAndGraysOut', async function() {
    await acceptExplainFre();
    await initPage();
    const toggle = getExplainToggle();
    assertTrue(!!toggle);
    assertTrue(toggle.checked);
    assertFalse(toggle.disabled);

    toggle.click();
    await microtasksFinished();
    assertFalse(
        PrefService.getInstance().getPref<boolean>(EXPLAIN_PREF_KEY).value);
    assertFalse(toggle.checked);
    // Turning it back on goes through the FRE.
    assertTrue(toggle.disabled);
  });

  test('ExplainToggleHiddenWhenExplainOff', async function() {
    loadTimeData.overrideValues(
        {enableQuickAnswersSelectionSuggestions: false});
    await acceptExplainFre();
    await initPage();
    assertFalse(!!getExplainToggle());
  });

  test('PreviewChipIcon', async function() {
    await initPage();
    assertTrue(isVisible(page.$.previewChipIcon));
    // <if expr="_google_chrome">
    assertEquals('settings-internal:spark', page.$.previewChipIcon.icon);

    page.remove();
    browserProxy.reset();
    loadTimeData.overrideValues({glicAssetsV2Enabled: true});
    await initPage();
    assertTrue(isVisible(page.$.previewChipIcon));
    assertEquals('settings-internal:sparkv2', page.$.previewChipIcon.icon);

    page.remove();
    browserProxy.reset();
    loadTimeData.overrideValues({enableGlicSelectionOverlayPromptBox: true});
    await initPage();
    assertTrue(isVisible(page.$.previewChipIcon));
    // </if>
    assertEquals('settings20:text-analysis', page.$.previewChipIcon.icon);
  });
});
