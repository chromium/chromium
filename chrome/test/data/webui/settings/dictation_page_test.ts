// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import type {SettingsDictationPageElement} from 'chrome://settings/lazy_load.js';
import {DictationBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefsBrowserProxy, PrefService, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestDictationBrowserProxy} from './test_dictation_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'browser.voice_typing_hotkey',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: '',
    },
  ];
}

suite('DictationPage', function() {
  let browserProxy: TestDictationBrowserProxy;
  let page: SettingsDictationPageElement;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      showAiPage: true,
      showDictationControl: true,
    });
    resetRouterForTesting();

    browserProxy = new TestDictationBrowserProxy();
    DictationBrowserProxyImpl.setInstance(browserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    page = document.createElement('settings-dictation-page');
    document.body.appendChild(page);
    await browserProxy.whenCalled('getDictationShortcut');
    await microtasksFinished();
  });

  test('ShortcutUpdateSuccess', async () => {
    browserProxy.setDictationShortcutResult(true);

    assertTrue(isVisible(page.$.shortcutInput));

    page.$.shortcutInput.fire('shortcut-updated', 'Ctrl+Shift+D');

    const shortcut = await browserProxy.whenCalled('setDictationShortcut');
    assertEquals('Ctrl+Shift+D', shortcut);
  });

  test('ShortcutUpdateFailedRevertsPref', async () => {
    browserProxy.setDictationShortcutResult(false);
    browserProxy.setDictationShortcutValue('Ctrl+Alt+D');
    await prefService.setPrefValue('browser.voice_typing_hotkey', 'Ctrl+Alt+D');

    assertTrue(isVisible(page.$.shortcutInput));

    page.$.shortcutInput.fire('shortcut-updated', 'InvalidShortcut');

    const shortcut = await browserProxy.whenCalled('setDictationShortcut');
    assertEquals('InvalidShortcut', shortcut);
    await microtasksFinished();

    // Verify element reverted shortcut property back to original pref value.
    assertEquals('Ctrl+Alt+D', page.$.shortcutInput.shortcut);
  });

  test('PrefChangeShowsFormattedShortcutFromBrowser', async () => {
    browserProxy.setDictationShortcutValue('⌃⇧D');
    await prefService.setPrefValue(
        'browser.voice_typing_hotkey', 'Ctrl+Shift+D');
    await microtasksFinished();

    assertEquals('⌃⇧D', page.$.shortcutInput.shortcut);
  });

  test('settingsPageLearnMoreShown', () => {
    assertTrue(isVisible(page.$.dictationLearnMoreLabel));
    assertEquals(
        'https://support.google.com/chrome?p=voice_typing',
        page.$.dictationLearnMoreLabel.href);
  });
});
