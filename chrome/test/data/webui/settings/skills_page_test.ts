// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import type {SettingsSkillsPageElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

suite('SkillsPage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsSkillsPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    // Reset pref to default.
    settingsPrefs.set('prefs.skills.enabled.value', true);
    openWindowProxy.reset();
  });

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-skills-page');
    subpage.prefs = settingsPrefs.prefs!;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  test('skillsToggle', async () => {
    await createPage();


    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    // Default should be true.
    assertTrue(subpage.getPref<boolean>('skills.enabled').value);
    assertTrue(toggle.checked);

    // Toggle to false.
    toggle.click();
    assertFalse(subpage.getPref<boolean>('skills.enabled').value);
    assertFalse(toggle.checked);

    // Toggle back to true.
    toggle.click();
    assertTrue(subpage.getPref<boolean>('skills.enabled').value);
    assertTrue(toggle.checked);
  });

  test('skillsGalleryLink', async function() {
    await createPage();

    const linkRow =
        subpage.shadowRoot!.querySelector<HTMLElement>('#skillsGalleryLink');
    assertTrue(!!linkRow);

    linkRow.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals('chrome://skills', url);
  });
});
