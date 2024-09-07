// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertFalse, assertTrue, assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {UserAnnotationsManagerProxy, SettingsAutofillPredictionImprovementsSectionElement} from 'chrome://settings/lazy_load.js';
import {UserAnnotationsManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on


suite('AutofillPredictionImprovementsSectionUiDisabledToggleTest', function() {
  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('testEntriesWithInitiallyDisabledToggle', async function() {
    const section = document.createElement(
        'settings-autofill-prediction-improvements-section');
    section.prefs = {
      autofill: {
        prediction_improvements: {
          enabled: {
            key: 'autofill_prediction_improvements.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
    };

    document.body.appendChild(section);
    await flushTasks();

    assertTrue(
        !section.shadowRoot!.querySelector('#entriesHeader'),
        'Entries are visible when ' +
            '"prefs.autofill_prediction_improvements.enabled is true"');
    assertTrue(
        !section.shadowRoot!.querySelector('#entries'),
        'Entries are visible when ' +
            '"prefs.autofill_prediction_improvements.enabled is true"');
  });
});

suite('AutofillPredictionImprovementsSectionUiTest', function() {
  class TestUserAnnotationsManagerProxyImpl extends TestBrowserProxy implements
      UserAnnotationsManagerProxy {
    constructor() {
      super(['getEntries']);
    }

    getEntries(): Promise<chrome.autofillPrivate.UserAnnotationsEntry[]> {
      this.methodCalled('getEntries');
      return Promise.resolve([
        {
          entryId: 1,
          key: 'Date of birth',
          value: '15/02/1989',
        },
        {
          entryId: 2,
          key: 'Frequent flyer program',
          value: 'Aadvantage',
        },
      ]);
    }
  }

  let section: SettingsAutofillPredictionImprovementsSectionElement;
  let entries: HTMLElement;
  let userAnnotationManager: TestUserAnnotationsManagerProxyImpl;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    userAnnotationManager = new TestUserAnnotationsManagerProxyImpl();
    UserAnnotationsManagerProxyImpl.setInstance(userAnnotationManager);

    section = document.createElement(
        'settings-autofill-prediction-improvements-section');
    section.prefs = {
      autofill: {
        prediction_improvements: {
          enabled: {
            key: 'autofill_prediction_improvements.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
        },
      },
    };
    document.body.appendChild(section);
    await flushTasks();

    const entriesQueried =
        section.shadowRoot!.querySelector<HTMLElement>('#entries');
    assertTrue(!!entriesQueried);
    entries = entriesQueried;

    assertTrue(!!section.shadowRoot!.querySelector('#entriesHeader'));
  });


  test('testEntriesListDataFromUserAnnotationsManager', async function() {
    await userAnnotationManager.whenCalled('getEntries');
    assertEquals(
        2, entries.querySelectorAll('.list-item').length,
        '2 entries come from TestUserAnnotationsManagerImpl.getEntries().');
  });

  test('testEntriesDisappearAfterToggleDisabling', async function() {
    // The toggle is initially enabled (see the setup() method), clicking it
    // disables the 'autofill_prediction_improvements.enabled' pref.
    section.$.prefToggle.click();
    await flushTasks();

    assertFalse(
        isVisible(section.shadowRoot!.querySelector('#entriesHeader')),
        'With the toggle disabled, the entries should be hidden');
    assertFalse(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'With the toggle disabled, the entries should be hidden');
  });
});
