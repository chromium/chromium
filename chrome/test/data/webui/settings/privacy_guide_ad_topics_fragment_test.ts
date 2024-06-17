// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {PrivacyGuideAdTopicsFragmentElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, PrivacySandboxBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPrivacySandboxBrowserProxy} from './test_privacy_sandbox_browser_proxy.js';

suite('AdTopicsFragment', function() {
  let fragment: PrivacyGuideAdTopicsFragmentElement;
  let settingsPrefs: SettingsPrefsElement;
  let testPrivacySandboxBrowserProxy: TestPrivacySandboxBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testPrivacySandboxBrowserProxy = new TestPrivacySandboxBrowserProxy();
    PrivacySandboxBrowserProxyImpl.setInstance(testPrivacySandboxBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));

    fragment = document.createElement('privacy-guide-ad-topics-fragment');
    fragment.prefs = settingsPrefs.prefs!;
    document.body.appendChild(fragment);

    return flushTasks();
  });

  test('TestAdTopicsPageContent', function() {
    const adTopicsToggle =
        fragment.shadowRoot!.querySelector('settings-toggle-button');
    const cardHeader =
        fragment.shadowRoot!.querySelector<HTMLElement>('.header-label-phase2');
    const descriptionHeaders =
        fragment.shadowRoot!.querySelectorAll<HTMLElement>(
            '.description-header');
    const descriptionItems =
        fragment.shadowRoot!.querySelectorAll('privacy-guide-description-item');
    assertTrue(isVisible(adTopicsToggle));
    assertEquals(
        loadTimeData.getString('privacyGuideAdTopicsToggleLabel'),
        adTopicsToggle!.label);
    assertTrue(isVisible(cardHeader));
    assertEquals(
        loadTimeData.getString('privacyGuideAdTopicsHeading'),
        cardHeader!.innerText);
    assertEquals(2, descriptionHeaders.length);
    assertTrue(isVisible(descriptionHeaders![0]!));
    assertEquals(
        loadTimeData.getString('columnHeadingWhenOn'),
        descriptionHeaders![0]!.innerText);
    assertTrue(isVisible(descriptionHeaders![1]!));
    assertEquals(
        loadTimeData.getString('columnHeadingConsider'),
        descriptionHeaders![1]!.innerText);
    assertEquals(4, descriptionItems.length);
    descriptionItems.forEach(item => {
      assertTrue(isVisible(item));
    });
    assertEquals(
        loadTimeData.getString('privacyGuideAdTopicsWhenOnBullet1'),
        descriptionItems![0]!.label);
    assertEquals(
        loadTimeData.getString('privacyGuideAdTopicsWhenOnBullet2'),
        descriptionItems![1]!.label);
    assertEquals(
        loadTimeData.getString('privacyGuideAdTopicsThingsToConsiderBullet1'),
        descriptionItems![2]!.label);
    assertEquals(
        loadTimeData.getString('privacyGuideAdTopicsThingsToConsiderBullet2'),
        descriptionItems![3]!.label);
  });

  test('TestAdTopicsPageToggle', function() {
    const adTopicsToggle =
        fragment.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!adTopicsToggle);
    adTopicsToggle.click();
    return testPrivacySandboxBrowserProxy.whenCalled('topicsToggleChanged');
  });
});
