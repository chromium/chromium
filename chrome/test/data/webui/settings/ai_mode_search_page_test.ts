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
          value: false,
        },
      },
    });
    assertFalse(toggle.checked);

    // Click toggle
    toggle.click();
    assertTrue(subpage.prefs['contextual_tasks']['share_open_tabs_every_thread']
                   .value);
    assertTrue(toggle.checked);

    // Click again
    toggle.click();
    assertFalse(
        subpage.prefs['contextual_tasks']['share_open_tabs_every_thread']
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
});
