// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsOfferWritingHelpPageElement} from 'chrome://settings/lazy_load.js';
import {COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF, COMPOSE_PROACTIVE_NUDGE_PREF} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

suite('ComposePage', function() {
  let page: SettingsOfferWritingHelpPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-offer-writing-help-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    return flushTasks();
  }

  // Test that interacting with the main toggle updates the corresponding pref.
  test('MainToggle', async () => {
    await createPage();
    page.setPrefValue(COMPOSE_PROACTIVE_NUDGE_PREF, false);

    const mainToggle = page.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!mainToggle);

    // Check disabled case.
    assertFalse(mainToggle.checked);

    // Check enabled case.
    mainToggle.click();
    assertEquals(true, page.getPref(COMPOSE_PROACTIVE_NUDGE_PREF).value);
    assertTrue(mainToggle.checked);
  });

  test('DisabledSitesListUpdate', function() {
    // "No sites added" message should be shown when list is empty.
    const noDisabledSitesLabel =
        page.shadowRoot!.querySelector('#noDisabledSitesLabel');
    assertTrue(!!noDisabledSitesLabel);
    assertTrue(isVisible(noDisabledSitesLabel));

    const disabledSites =
        page.shadowRoot!.querySelectorAll('div[role=listitem]');
    assertEquals(0, disabledSites.length);

    // Adding an entry to the pref should populate the list and remove the "No
    // sites added" message.
    page.setPrefDictEntry(
        COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF, 'foo', 'bar');
    flush();
    assertFalse(isVisible(noDisabledSitesLabel));
    const newSites = page.shadowRoot!.querySelectorAll('div[role=listitem]');
    assertEquals(1, newSites.length);
  });

  test('DisabledSitesListDelete', function() {
    page.setPrefDictEntry(
        COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF, 'foo', 'foo');
    page.setPrefDictEntry(
        COMPOSE_PROACTIVE_NUDGE_DISABLED_SITES_PREF, 'bar', 'bar');
    flush();

    const sites = page.shadowRoot!.querySelectorAll('div[role=listitem]');
    assertEquals(2, sites.length);
    // Check the content of the first list item.
    const entry1 = sites[0]!.firstChild!.textContent;
    assertTrue(!!entry1);
    assertEquals('foo', entry1);
    // Check the content of the second list item.
    const entry2 = sites[1]!.firstChild!.textContent;
    assertTrue(!!entry2);
    assertEquals('bar', entry2);

    // Get the delete button of the second list item and click to remove.
    const button = sites[1]!.lastChild as HTMLElement;
    assertTrue(!!button);
    button.click();
    flush();
    const newSites = page.shadowRoot!.querySelectorAll('div[role=listitem]');
    assertEquals(1, newSites.length);
    const remainingEntry = newSites[0]!.firstChild!.textContent;
    assertTrue(!!remainingEntry);
    assertEquals('foo', remainingEntry);
  });

  // TODO(b/335014680): Remove after EnableComposeProactiveNudge is launched.
  test('FeatureVisibility', async () => {
    // Case 1, Compose proactive nudge is disabled, HelpMeWrite section should
    // be visible, OfferWritingHelp toggle should be hidden.
    loadTimeData.overrideValues({enableComposeProactiveNudge: false});
    await createPage();

    assertTrue(isChildVisible(page, '#helpMeWriteLabel'));
    const toggle1 =
        page.shadowRoot!.querySelector<HTMLElement>('settings-toggle-button');
    assertFalse(!!toggle1);

    // Case 2, Compose proactive nudge is enabled, HelpMeWrite section should be
    // visible, OfferWritingHelp toggle should be visible.
    loadTimeData.overrideValues({enableComposeProactiveNudge: true});
    await createPage();

    assertTrue(isChildVisible(page, '#helpMeWriteLabel'));
    const toggle2 =
        page.shadowRoot!.querySelector<HTMLElement>('settings-toggle-button');
    assertTrue(!!toggle2);
    assertTrue(isVisible(toggle2));

    // Test that a separator is shown for the OfferWritingHelp toggle when the
    // Refresh flag is enabled.
    assertTrue(toggle2.classList.contains('hr'));
  });

  test('ComposeLearnMore', async () => {
    await createPage();

    const learnMoreLink = page.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href, loadTimeData.getString('composeLearnMorePageURL'));
  });
});

// TODO(crbug.com/362225975): Remove after AiSettingsPageRefresh is launched.
suite('ComposePageRefreshDisabled', function() {
  let page: SettingsOfferWritingHelpPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableComposeProactiveNudge: true,
      enableAiSettingsPageRefresh: false,
    });

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-offer-writing-help-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    return flushTasks();
  }

  test('FeatureVisibility', async () => {
    // Refresh flag is disabled, HelpMeWrite section and OfferWritingHelp toggle
    // separator should be hidden.
    await createPage();

    assertFalse(isChildVisible(page, '#helpMeWriteLabel'));
    const toggle =
        page.shadowRoot!.querySelector<HTMLElement>('settings-toggle-button');
    assertTrue(!!toggle);
    assertTrue(isVisible(toggle));
    assertFalse(toggle.classList.contains('hr'));
  });
});
