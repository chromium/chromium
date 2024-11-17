// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertTrue, assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {SettingsSimpleConfirmationDialogElement, SettingsAutofillPredictionImprovementsSectionElement} from 'chrome://settings/lazy_load.js';
import {UserAnnotationsManagerProxyImpl} from 'chrome://settings/lazy_load.js';

import {TestUserAnnotationsManagerProxyImpl} from './test_user_annotations_manager_proxy.js';

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
            key: 'autofill.prediction_improvements.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
    };

    document.body.appendChild(section);
    await flushTasks();

    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entriesHeader')),
        'With the toggle disabled, the entries should be visible');
    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'With the toggle disabled, the entries should be visible');
  });
});

suite('AutofillPredictionImprovementsSectionUiTest', function() {
  let section: SettingsAutofillPredictionImprovementsSectionElement;
  let entries: HTMLElement;
  let userAnnotationManager: TestUserAnnotationsManagerProxyImpl;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    userAnnotationManager = new TestUserAnnotationsManagerProxyImpl();
    UserAnnotationsManagerProxyImpl.setInstance(userAnnotationManager);

    const testEntries: chrome.autofillPrivate.UserAnnotationsEntry[] = [
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
    ];
    userAnnotationManager.setEntries(testEntries);

    section = document.createElement(
        'settings-autofill-prediction-improvements-section');
    section.prefs = {
      autofill: {
        prediction_improvements: {
          enabled: {
            key: 'autofill.prediction_improvements.enabled',
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

  test('testEntriesDoNotDisappearAfterToggleDisabling', async function() {
    // The toggle is initially enabled (see the setup() method), clicking it
    // disables the 'autofill.prediction_improvements.enabled' pref.
    section.$.prefToggle.click();
    await flushTasks();

    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entriesHeader')),
        'With the toggle disabled, the entries should be visible');
    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'With the toggle disabled, the entries should be visible');
  });

  test('testCancelingEntryDeleteDialog', async function() {
    const deleteButton = entries.querySelector<HTMLElement>(
        '.list-item:nth-of-type(1) cr-icon-button');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    const deleteEntryDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#deleteEntryDialog');
    assertTrue(
        !!deleteEntryDialog,
        '#deleteEntryDialog should be in DOM after clicking delete button');
    deleteEntryDialog.$.cancel.click();
    await flushTasks();

    assertEquals(0, userAnnotationManager.getCallCount('deleteEntry'));
    assertEquals(
        2, entries.querySelectorAll('.list-item').length,
        'The 2 entries should remain intact.');
  });

  test('testConfirmingEntryDeleteDialog', async function() {
    const deleteButton = entries.querySelector<HTMLElement>(
        '.list-item:nth-of-type(1) cr-icon-button');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    const deleteEntryDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#deleteEntryDialog');
    assertTrue(
        !!deleteEntryDialog,
        '#deleteEntryDialog should be in DOM after clicking delete button');
    deleteEntryDialog.$.confirm.click();

    const entryId = await userAnnotationManager.whenCalled('deleteEntry');
    await flushTasks();

    assertEquals(1, userAnnotationManager.getCallCount('deleteEntry'));
    assertEquals(1, entryId);
    assertEquals(
        1, entries.querySelectorAll('.list-item').length,
        'One of the 2 entries should be removed');
  });

  test('testCancelingDeleteAllEntriesDialog', async function() {
    const deleteButton =
        section.shadowRoot!.querySelector<HTMLElement>('#deleteAllEntries');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    const deleteAllEntriesDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#deleteAllEntriesDialog');
    assertTrue(
        !!deleteAllEntriesDialog, '#deleteAllEntriesDialog should be in DOM');
    deleteAllEntriesDialog.$.cancel.click();
    await flushTasks();

    assertEquals(0, userAnnotationManager.getCallCount('deleteAllEntries'));
    assertEquals(
        entries.querySelectorAll('.list-item').length, 2,
        'The 2 entries should remain intact.');
  });

  test('testConfirmingDeleteAllEntriesDialog', async function() {
    const deleteButton =
        section.shadowRoot!.querySelector<HTMLElement>('#deleteAllEntries');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    const deleteAllEntriesDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#deleteAllEntriesDialog');
    assertTrue(
        !!deleteAllEntriesDialog, '#deleteAllEntriesDialog should be in DOM');
    deleteAllEntriesDialog.$.confirm.click();

    await userAnnotationManager.whenCalled('deleteAllEntries');
    await flushTasks();

    assertEquals(1, userAnnotationManager.getCallCount('deleteAllEntries'));
    assertEquals(
        entries.querySelectorAll('.list-item').length, 1,
        'All entries should be removed (-2), the "no entries" message shows ' +
            'up instead (+1).');
    assertTrue(
        isVisible(entries.querySelector('#entriesNone')),
        'The "no entries" message shows up when the list is empty');
  });
});

suite('AutofillPredictionImprovementsSectionToggleTest', function() {
  let section: SettingsAutofillPredictionImprovementsSectionElement;
  let userAnnotationManager: TestUserAnnotationsManagerProxyImpl;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({autofillPredictionBootstrappingEnabled: true});

    userAnnotationManager = new TestUserAnnotationsManagerProxyImpl();
    UserAnnotationsManagerProxyImpl.setInstance(userAnnotationManager);

    section = document.createElement(
        'settings-autofill-prediction-improvements-section');
    section.prefs = {
      autofill: {
        prediction_improvements: {
          enabled: {
            key: 'autofill.prediction_improvements.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
    };
    document.body.appendChild(section);
    await flushTasks();
  });

  test('testTriggerBootstrappingCalledWhenConditionsMet', async function() {
    loadTimeData.overrideValues({autofillPredictionBootstrappingEnabled: true});
    userAnnotationManager.setEntries([]);

    userAnnotationManager.reset();

    section.$.prefToggle.click();
    await flushTasks();

    await userAnnotationManager.whenCalled('hasEntries');
    await userAnnotationManager.whenCalled('triggerBootstrapping');

    assertEquals(
        1, userAnnotationManager.getCallCount('triggerBootstrapping'),
        'triggerBootstrapping should be called when all conditions are met');
  });

  test(
      'testTriggerBootstrappingNotCalledWhenBootstrappingDisabled',
      async function() {
        loadTimeData.overrideValues(
            {autofillPredictionBootstrappingEnabled: false});
        userAnnotationManager.setEntries([]);

        userAnnotationManager.reset();

        section.$.prefToggle.click();
        await flushTasks();

        await userAnnotationManager.whenCalled('hasEntries');

        assertEquals(
            0, userAnnotationManager.getCallCount('triggerBootstrapping'),
            'triggerBootstrapping shouldn\'t be called if feature is disabled');
      });

  test('testTriggerBootstrappingNotCalledWhenToggleDisabled', async function() {
    loadTimeData.overrideValues({autofillPredictionBootstrappingEnabled: true});
    userAnnotationManager.setEntries([]);

    userAnnotationManager.reset();

    section.setPrefValue('autofill.prediction_improvements.enabled', true);
    await flushTasks();

    section.$.prefToggle.click();
    await flushTasks();

    await userAnnotationManager.whenCalled('hasEntries');

    assertEquals(
        0, userAnnotationManager.getCallCount('triggerBootstrapping'),
        'triggerBootstrapping should not be called when toggle is disabled');
  });

  test('testTriggerBootstrappingNotCalledWhenHasEntries', async function() {
    loadTimeData.overrideValues({autofillPredictionBootstrappingEnabled: true});
    userAnnotationManager.setEntries([
      {
        entryId: 1,
        key: 'Test Key',
        value: 'Test Value',
      },
    ]);

    userAnnotationManager.reset();

    section.$.prefToggle.click();
    await flushTasks();

    await userAnnotationManager.whenCalled('hasEntries');

    assertEquals(
        0, userAnnotationManager.getCallCount('triggerBootstrapping'),
        'triggerBootstrapping should not be called when entries exist');
  });

  test('testTriggerBootstrappingNotCalledWhenComponentDisabled', async function() {
    loadTimeData.overrideValues({autofillPredictionBootstrappingEnabled: true});
    userAnnotationManager.setEntries([]);

    userAnnotationManager.reset();

    section.disabled = true;

    section.$.prefToggle.click();
    await flushTasks();

    assertEquals(
        0, userAnnotationManager.getCallCount('triggerBootstrapping'),
        'triggerBootstrapping should not be called when component is disabled');
  });

  test('testGetEntriesCalledWhenBootstrappingAddsEntries', async function() {
    loadTimeData.overrideValues({autofillPredictionBootstrappingEnabled: true});

    userAnnotationManager.setEntries([]);
    userAnnotationManager.setEntriesBootstrapped(true);

    userAnnotationManager.reset();

    section.$.prefToggle.click();
    await flushTasks();

    await userAnnotationManager.whenCalled('hasEntries');
    await userAnnotationManager.whenCalled('triggerBootstrapping');
    await userAnnotationManager.whenCalled('getEntries');

    assertEquals(
        1, userAnnotationManager.getCallCount('getEntries'),
        'getEntries should be called if bootstrapping added entries');
  });
});
