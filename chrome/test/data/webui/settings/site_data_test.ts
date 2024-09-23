// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSiteDataElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

// Name of the cookie default content setting pref.
const PREF_NAME = 'generated.cookie_default_content_setting';

suite('SiteDataTest', function() {
  let page: SettingsSiteDataElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-data');
    page.prefs = settingsPrefs.prefs!;
    page.set('prefs.' + PREF_NAME + '.value', ContentSetting.ALLOW);
    document.body.appendChild(page);
    return microtasksFinished();
  });

  teardown(function() {
    page.remove();
  });

  function getDefaultBlockDialog() {
    return page.shadowRoot!.querySelector('#defaultBlockDialog');
  }

  test('DefaultSettingAllowUpdatesPref', async function() {
    // Start from a different state than 'allow'.
    page.$.defaultSessionOnly.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(
        page.getPref(PREF_NAME + '.value'), ContentSetting.SESSION_ONLY);

    page.$.defaultAllow.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(page.getPref(PREF_NAME + '.value'), ContentSetting.ALLOW);
  });

  test('DefaultSettingSessionOnlyUpdatesPref', async function() {
    // Default is 'allow'.
    assertEquals(page.getPref(PREF_NAME + '.value'), ContentSetting.ALLOW);

    page.$.defaultSessionOnly.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(
        page.getPref(PREF_NAME + '.value'), ContentSetting.SESSION_ONLY);
  });

  test('DefaultSettingBlockUpdatesPref', async function() {
    // Default is 'allow'.
    assertEquals(page.getPref(PREF_NAME + '.value'), ContentSetting.ALLOW);

    page.$.defaultBlock.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    // Changing to block requires confirmation in the dialog to take effect.
    assertTrue(!!getDefaultBlockDialog());
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#defaultBlockDialogConfirm')!.click();
    await flushTasks();

    assertFalse(!!getDefaultBlockDialog());
    assertEquals(page.getPref(PREF_NAME + '.value'), ContentSetting.BLOCK);
  });

  test('BlockSiteDataFromAllowDontConfirmDialog', async function() {
    // Default is 'allow'.
    assertEquals(page.getPref(PREF_NAME + '.value'), ContentSetting.ALLOW);

    page.$.defaultBlock.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertTrue(!!getDefaultBlockDialog());
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#defaultBlockDialogCancel')!.click();
    await flushTasks();

    assertFalse(!!getDefaultBlockDialog());
    assertEquals(page.getPref(PREF_NAME + '.value'), ContentSetting.ALLOW);
    assertEquals(page.$.defaultGroup.selected, ContentSetting.ALLOW);
  });

  test('BlockSiteDataFromSessionOnlyDontConfirmDialog', async function() {
    page.$.defaultSessionOnly.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(
        page.getPref(PREF_NAME + '.value'), ContentSetting.SESSION_ONLY);

    page.$.defaultBlock.click();
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertTrue(!!getDefaultBlockDialog());
    page.shadowRoot!.querySelector<HTMLElement>(
                        '#defaultBlockDialogCancel')!.click();
    await flushTasks();

    assertFalse(!!getDefaultBlockDialog());
    assertEquals(
        page.getPref(PREF_NAME + '.value'), ContentSetting.SESSION_ONLY);
    assertEquals(page.$.defaultGroup.selected, ContentSetting.SESSION_ONLY);
  });

  test('PrefChangesUpdateDefaultSetting', async function() {
    // Default is 'allow'.
    assertEquals(page.$.defaultGroup.selected, ContentSetting.ALLOW);

    page.set('prefs.' + PREF_NAME + '.value', ContentSetting.SESSION_ONLY);
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(page.$.defaultGroup.selected, ContentSetting.SESSION_ONLY);

    page.set('prefs.' + PREF_NAME + '.value', ContentSetting.BLOCK);
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(page.$.defaultGroup.selected, ContentSetting.BLOCK);

    page.set('prefs.' + PREF_NAME + '.value', ContentSetting.ALLOW);
    await eventToPromise('selected-changed', page.$.defaultGroup);
    assertEquals(page.$.defaultGroup.selected, ContentSetting.ALLOW);
  });

  test('ExceptionListsReadOnly', function() {
    // Check all exception lists are read only when the preference
    // reports as managed.
    page.set('prefs.' + PREF_NAME, {
      value: ContentSetting.ALLOW,
      type: chrome.settingsPrivate.PrefType.STRING,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    let exceptionLists = page.shadowRoot!.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertTrue(!!list.readOnlyList);
    }

    // Return preference to unmanaged state and check all exception lists
    // are no longer read only.
    page.set('prefs.' + PREF_NAME, {
      value: ContentSetting.ALLOW,
      type: chrome.settingsPrivate.PrefType.STRING,
    });
    exceptionLists = page.shadowRoot!.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);
    for (const list of exceptionLists) {
      assertFalse(!!list.readOnlyList);
    }
  });

  test('ExceptionsSearch', async function() {
    while (siteSettingsBrowserProxy.getCallCount('getExceptionList') < 3) {
      await flushTasks();
    }
    siteSettingsBrowserProxy.resetResolver('getExceptionList');

    const exceptionPrefs = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.COOKIES,
          [
            createRawSiteException('http://foo-allow.com', {
              embeddingOrigin: '',
            }),
            createRawSiteException('http://foo-session.com', {
              embeddingOrigin: '',
              setting: ContentSetting.SESSION_ONLY,
            }),
            createRawSiteException('http://foo-block.com', {
              embeddingOrigin: '',
              setting: ContentSetting.BLOCK,
            }),
          ]),
    ]);
    page.searchTerm = 'foo';
    siteSettingsBrowserProxy.setPrefs(exceptionPrefs);
    while (siteSettingsBrowserProxy.getCallCount('getExceptionList') < 3) {
      await flushTasks();
    }
    flush();

    const exceptionLists = page.shadowRoot!.querySelectorAll('site-list');
    assertEquals(exceptionLists.length, 3);

    for (const list of exceptionLists) {
      assertTrue(isChildVisible(list, 'site-list-entry'));
    }

    page.searchTerm = 'unrelated.com';
    flush();

    for (const list of exceptionLists) {
      assertFalse(isChildVisible(list, 'site-list-entry'));
    }
  });

  test('ExceptionListsHaveCorrectCookieExceptionType', function() {
    const allowExceptionsList =
        page.shadowRoot!.querySelector('#allowExceptionsList');
    assertTrue(!!allowExceptionsList);
    assertEquals(
        'site-data',
        allowExceptionsList.getAttribute('cookies-exception-type'));

    const sessionOnlyExceptionsList =
        page.shadowRoot!.querySelector('#sessionOnlyExceptionsList');
    assertTrue(!!sessionOnlyExceptionsList);
    assertEquals(
        'site-data',
        sessionOnlyExceptionsList.getAttribute('cookies-exception-type'));

    const blockExceptionsList =
        page.shadowRoot!.querySelector('#blockExceptionsList');
    assertTrue(!!blockExceptionsList);
    assertEquals(
        'combined', blockExceptionsList.getAttribute('cookies-exception-type'));
  });
});
