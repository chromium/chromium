// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import type {SettingsOmniboxEverywhereSectionElement} from 'chrome://settings/settings.js';
import {OmniboxEverywhereBrowserProxyImpl, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestOmniboxEverywhereBrowserProxy} from './test_omnibox_everywhere_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('OmniboxEverywhereSectionTest', function() {
  let section: SettingsOmniboxEverywhereSectionElement;
  let browserProxy: TestOmniboxEverywhereBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestOmniboxEverywhereBrowserProxy();
    OmniboxEverywhereBrowserProxyImpl.setInstance(browserProxy);

    const fakePrefs = [
      {
        key: 'omnibox_everywhere.enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
      {
        key: 'omnibox_everywhere.hotkey_enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'omnibox_everywhere.show_shortcuts',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ];
    const prefsBrowserProxy = new TestPrefsBrowserProxy(fakePrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    section = document.createElement('settings-omnibox-everywhere-section');
    document.body.appendChild(section);
    await microtasksFinished();
  });

  test('InitialState', function() {
    const mainToggle = section.shadowRoot.querySelector('#mainToggle');
    const collapse =
        section.shadowRoot.querySelector<CrCollapseElement>('#expandedContent');
    const shortcutInput = section.shadowRoot.querySelector('#shortcutInput');
    const showShortcutsToggle =
        section.shadowRoot.querySelector('#showShortcutsToggle');

    assertTrue(isVisible(mainToggle));
    assertTrue(!!collapse);
    assertFalse(collapse.opened);
    assertTrue(!!shortcutInput);
    assertTrue(!!showShortcutsToggle);

    assertFalse(
        prefService.getPref<boolean>('omnibox_everywhere.enabled').value);
    assertTrue(prefService.getPref<boolean>('omnibox_everywhere.show_shortcuts')
                   .value);
  });

  test('MainToggleChange', async function() {
    const mainToggle =
        section.shadowRoot.querySelector<HTMLElement>('#mainToggle');
    const collapse =
        section.shadowRoot.querySelector<CrCollapseElement>('#expandedContent');
    assertTrue(!!mainToggle);
    assertTrue(!!collapse);
    assertFalse(collapse.opened);

    mainToggle.click();
    await microtasksFinished();

    assertTrue(
        prefService.getPref<boolean>('omnibox_everywhere.enabled').value);
    assertTrue(collapse.opened);
  });

  test('ShowShortcutsToggleChange', async function() {
    const showShortcutsToggle =
        section.shadowRoot.querySelector<HTMLElement>('#showShortcutsToggle');
    assertTrue(!!showShortcutsToggle);

    showShortcutsToggle.click();
    await microtasksFinished();

    assertFalse(
        prefService.getPref<boolean>('omnibox_everywhere.show_shortcuts')
            .value);
  });

  test('ShortcutUpdated', async function() {
    const shortcutInput = section.shadowRoot.querySelector('#shortcutInput');
    assertTrue(!!shortcutInput);

    shortcutInput.dispatchEvent(new CustomEvent('shortcut-updated', {
      bubbles: true,
      composed: true,
      detail: 'Ctrl+Shift+Space',
    }));
    await microtasksFinished();

    const updatedShortcut =
        await browserProxy.whenCalled('setOmniboxEverywhereShortcut');
    assertEquals('Ctrl+Shift+Space', updatedShortcut);
  });

  test('InputCaptureChange', async function() {
    const shortcutInput = section.shadowRoot.querySelector('#shortcutInput');
    assertTrue(!!shortcutInput);

    shortcutInput.dispatchEvent(new CustomEvent('input-capture-change', {
      bubbles: true,
      composed: true,
      detail: true,
    }));
    await microtasksFinished();

    const shouldSuspend = await browserProxy.whenCalled(
        'setOmniboxEverywhereShortcutSuspensionState');
    assertTrue(shouldSuspend);
  });

  test('DisconnectedResumesSuspension', async function() {
    section.remove();
    await microtasksFinished();

    const shouldSuspend = await browserProxy.whenCalled(
        'setOmniboxEverywhereShortcutSuspensionState');
    assertFalse(shouldSuspend);
  });
});
