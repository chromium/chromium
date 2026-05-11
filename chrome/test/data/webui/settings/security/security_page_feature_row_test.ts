// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SecurityPageFeatureRowElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('securityPageFeatureRow', function() {
  let securityPageFeatureRow: SecurityPageFeatureRowElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    const fakePref = [{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }];
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(new FakeSettingsPrivate(fakePref));
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    securityPageFeatureRow =
        document.createElement('security-page-feature-row');
    securityPageFeatureRow.pref = settingsPrefs.get('prefs.test');
    securityPageFeatureRow.icon = 'settings20:warning_outline';

    document.body.appendChild(securityPageFeatureRow);
    flush();
  });

  function getToggleButton(): HTMLElement|null {
    return securityPageFeatureRow.shadowRoot!
        .querySelector<SettingsToggleButtonElement>('#toggleButton');
  }

  test('RowClickExpandsAndCollapses', async function() {
    const collapse =
        securityPageFeatureRow.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(collapse.opened);

    // Expand the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);
    assertTrue(collapse.opened);

    // Collapse the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(collapse.opened);
  });

  test('ToggleClickEnablesAndDisablesFeature', async function() {
    assertFalse(securityPageFeatureRow.pref.value);

    // Expand the feature row in order to see the toggle.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);

    // Click the toggle to enable the feature.
    getToggleButton()!.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.pref.value);

    // Verify clicking the toggle did NOT collapsed the row.
    assertTrue(securityPageFeatureRow.expanded);

    // Disable the feature again.
    getToggleButton()!.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.pref.value);
    assertTrue(securityPageFeatureRow.expanded);
  });

  test('RowClickShowsAndHidesToggle', async function() {
    // Since the row starts off collapsed, the toggle shouldn't be visible.
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(isChildVisible(securityPageFeatureRow, '#toggleButton'));

    // Expand the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);

    // Check that toggle is visible
    assertTrue(isChildVisible(securityPageFeatureRow, '#toggleButton'));

    // Collapse the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);

    // Check that toggle is NOT visible.
    assertFalse(isChildVisible(securityPageFeatureRow, '#toggleButton'));
  });

  test('StateLabelIsVisibleWhenCollapsed', async function() {
    // The row is collapsed by default, so the state label should be visible.
    assertFalse(securityPageFeatureRow.expanded);
    let stateLabel =
        securityPageFeatureRow.$.expandButton.querySelector<HTMLElement>(
            '#stateLabel');
    assertTrue(!!stateLabel);
    assertTrue(stateLabel.offsetParent !== null);

    // Expand the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);

    // The state label should now be hidden aka null.
    stateLabel = securityPageFeatureRow.$.expandButton.shadowRoot
                     .querySelector<HTMLElement>('#stateLabel');
    assertTrue(!stateLabel);

    // Collapse the feature row again.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);

    // The state label should be visible again.
    stateLabel =
        securityPageFeatureRow.$.expandButton.querySelector<HTMLElement>(
            '#stateLabel');
    assertTrue(!!stateLabel);
    assertTrue(stateLabel.offsetParent !== null);
  });

  test('IconDoesNotTransitionOnLoad', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    securityPageFeatureRow =
        document.createElement('security-page-feature-row');
    securityPageFeatureRow.pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    securityPageFeatureRow.icon = 'settings20:warning_outline';
    securityPageFeatureRow.iconVisible = true;

    document.body.appendChild(securityPageFeatureRow);
    const icon = securityPageFeatureRow.shadowRoot!.querySelector('#icon');
    assertTrue(!!icon);

    let transitionCreated = false;
    icon.addEventListener('transitionrun', () => {
      transitionCreated = true;
    });

    assertFalse(
        icon.classList.contains('enable-transition'),
        'enable-transition class should not be present');
    await flushTasks();

    assertFalse(transitionCreated, 'Icon should not transition on load');
    const computedStyle = getComputedStyle(icon);
    assertEquals('1', computedStyle.opacity);
    assertEquals('20px', computedStyle.width);
  });

  test('IconTransitionsOnToggleAfterExpansion', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    securityPageFeatureRow =
        document.createElement('security-page-feature-row');
    securityPageFeatureRow.pref = {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    securityPageFeatureRow.icon = 'settings20:warning_outline';
    securityPageFeatureRow.iconVisible = true;

    document.body.appendChild(securityPageFeatureRow);
    const icon = securityPageFeatureRow.shadowRoot!.querySelector('#icon');
    assertTrue(!!icon);

    let transitionCreated = false;
    icon.addEventListener('transitionrun', () => {
      transitionCreated = true;
    });
    await flushTasks();

    assertFalse(transitionCreated, 'Icon should not transition on load');
    const loadStyle = getComputedStyle(icon);
    assertEquals('1', loadStyle.opacity);
    assertEquals('20px', loadStyle.width);

    // Expand the row to add/enable the transition styling
    securityPageFeatureRow.$.expandButton.click();
    await flushTasks();
    assertTrue(securityPageFeatureRow.expanded);

    securityPageFeatureRow.iconVisible = false;
    await new Promise<void>(resolve => {
      icon.addEventListener('transitionend', (e: Event) => {
        // Wait for the longest transition (opacity) to finish.
        if ((e as TransitionEvent).propertyName === 'opacity') {
          resolve();
        }
      });
    });
    await flushTasks();

    assertTrue(
        transitionCreated, 'Icon should transition on visibility changes');
    const hiddenStyle = getComputedStyle(icon);
    assertEquals('0', hiddenStyle.opacity);
    assertEquals('0px', hiddenStyle.width);
  });

  test('IconVisibility', async function() {
    securityPageFeatureRow.iconVisible = true;
    await flushTasks();
    assertTrue(
        isChildVisible(securityPageFeatureRow, '#icon'),
        'Icon should be visible');

    securityPageFeatureRow.iconVisible = false;
    await flushTasks();
    assertFalse(
        isChildVisible(securityPageFeatureRow, '#icon'),
        'Icon should not be visible');
  });
});
