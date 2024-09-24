// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertFalse, assertTrue, assertEquals} from 'chrome://webui-test/chai_assert.js';
import type {UserAnnotationsManagerProxy, SettingsSimpleConfirmationDialogElement, SettingsAutofillPredictionImprovementsSectionElement} from 'chrome://settings/lazy_load.js';
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
      super(['getEntries', 'deleteEntry', 'deleteAllEntries']);
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

    deleteEntry(entryId: number): void {
      this.methodCalled('deleteEntry', entryId);
    }

    deleteAllEntries(): void {
      this.methodCalled('deleteAllEntries');
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

    await userAnnotationManager.whenCalled('deleteEntry');
    await flushTasks();

    assertEquals(1, userAnnotationManager.getCallCount('deleteEntry'));
    assertEquals(1, userAnnotationManager.getArgs('deleteEntry')[0]);
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
    assertTrue(!!deleteAllEntriesDialog, '#deleteEntryDialog should be in DOM');
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
        'The "no entries" messages shows up when the list is empty');
  });
});
