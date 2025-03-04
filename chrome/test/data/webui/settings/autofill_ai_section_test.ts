// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import type {SettingsAutofillAiAddOrEditDialogElement, SettingsSimpleConfirmationDialogElement, SettingsAutofillAiSectionElement} from 'chrome://settings/lazy_load.js';
import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

// TODO(crbug.com/393318914): Parameterize this suite and also test that the add
// button is disabled accordingly.
suite('AutofillAiSectionUiEligbilityTest', function() {
  let section: SettingsAutofillAiSectionElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    section = document.createElement('settings-autofill-ai-section');
    // The toggle is turned off.
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
  });

  test('testEntriesWithEligibleUserAndTurnedOffToggle', async function() {
    section.ineligibleUser = false;

    document.body.appendChild(section);
    await flushTasks();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#prefToggle')!;
    assertFalse(
        toggle.disabled,
        'The toggle should be enabled if the user is eligible');
    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'The entries should always be visible');
  });

  test('testEntriesWithNotEligibleUserAndTurnedOffToggle', async function() {
    section.ineligibleUser = true;

    document.body.appendChild(section);
    await flushTasks();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#prefToggle')!;
    assertTrue(
        toggle.disabled,
        'The toggle should be disabled if the user is ineligible');
    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'The entries should always be visible');
  });
});

suite('AutofillAiSectionUiTest', function() {
  let section: SettingsAutofillAiSectionElement;
  let entitiesListElement: HTMLElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let testEntity: chrome.autofillPrivate.EntityInstance;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    testEntity = {
      type: {
        typeName: 3,
        typeNameAsString: 'Car',
        addEntityString: 'Add car',
        editEntityString: 'Edit car',
      },
      attributes: [
        {
          type: {
            typeName: 8,
            typeNameAsString: 'Owner',
          },
          value: 'Mark Nolan',
        },
        {
          type: {
            typeName: 10,
            typeNameAsString: 'Registration',
          },
          value: 'ABCDE123',
        },
      ],
      guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
      nickname: 'My car',
    };
    const testEntityInstancesWithLabels:
        chrome.autofillPrivate.EntityInstanceWithLabels[] = [
      {
        guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
        entityLabel: 'Toyota',
        entitySubLabel: 'Car',
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        entityLabel: 'John Doe',
        entitySubLabel: 'Passport',
      },
    ];
    entityDataManager.setloadEntityInstancesResponse(
        testEntityInstancesWithLabels);

    section = document.createElement('settings-autofill-ai-section');
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

    const entitiesQueried =
        section.shadowRoot!.querySelector<HTMLElement>('#entries');
    assertTrue(!!entitiesQueried);
    entitiesListElement = entitiesQueried;

    assertTrue(!!section.shadowRoot!.querySelector('#entriesHeader'));
  });

  test('testEntitiesLoaded', async function() {
    await entityDataManager.whenCalled('loadEntityInstances');
    const listItems =
        entitiesListElement.querySelectorAll<HTMLElement>('.list-item');

    assertEquals(
        3, listItems.length, '2 entities and a hidden element were loaded.');
    assertTrue(listItems[0]!.textContent!.includes('Toyota'));
    assertTrue(listItems[1]!.textContent!.includes('John Doe'));
    assertTrue(listItems[2]!.hidden);
  });

  test('testRemoveEntityConfirmed', async function() {
    const actionMenuButton =
        entitiesListElement.querySelector<HTMLElement>('#moreButton');
    assertTrue(!!actionMenuButton);
    actionMenuButton.click();
    await flushTasks();

    const deleteButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuRemoveEntity');

    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    const removeEntityDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#removeEntityDialog');
    assertTrue(!!removeEntityDialog);

    removeEntityDialog.$.confirm.click();
    const guid = await entityDataManager.whenCalled('removeEntityInstance');
    await flushTasks();

    assertEquals(1, entityDataManager.getCallCount('removeEntityInstance'));
    assertEquals('e4bbe384-ee63-45a4-8df3-713a58fdc181', guid);

    const listItems =
        entitiesListElement.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(
        2, listItems.length,
        'only one entity and a hidden element should be present.');
    assertTrue(listItems[0]!.textContent!.includes('John Doe'));
    assertTrue(listItems[1]!.hidden);
  });

  test('testRemoveEntityCancelled', async function() {
    const actionMenuButton =
        entitiesListElement.querySelector<HTMLElement>('#moreButton');
    assertTrue(!!actionMenuButton);
    actionMenuButton.click();
    await flushTasks();

    const deleteButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuRemoveEntity');
    assertTrue(!!deleteButton);
    deleteButton.click();
    await flushTasks();

    const removeEntityDialog =
        section.shadowRoot!
            .querySelector<SettingsSimpleConfirmationDialogElement>(
                '#removeEntityDialog');
    assertTrue(!!removeEntityDialog);
    removeEntityDialog.$.cancel.click();
    await flushTasks();

    assertEquals(0, entityDataManager.getCallCount('removeEntityInstance'));

    const listItems =
        entitiesListElement.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(
        3, listItems.length,
        '2 entities and a hidden element should still be present.');
    assertTrue(listItems[0]!.textContent!.includes('Toyota'));
    assertTrue(listItems[1]!.textContent!.includes('John Doe'));
    assertTrue(listItems[2]!.hidden);
  });

  test('testEditEntityDialogOpenAndConfirm', async function() {
    entityDataManager.setGetEntityInstanceByGuidResponse(testEntity);

    const actionMenuButton =
        entitiesListElement.querySelector<HTMLElement>('#moreButton');
    assertTrue(!!actionMenuButton);
    actionMenuButton.click();
    await flushTasks();

    const editButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuEditEntity');

    assertTrue(!!editButton);
    editButton.click();
    await flushTasks();

    const editEntityDialog =
        section.shadowRoot!
            .querySelector<SettingsAutofillAiAddOrEditDialogElement>(
                '#addOrEditEntityDialog');
    assertTrue(!!editEntityDialog);
    assertDeepEquals(testEntity, editEntityDialog.entity);

    // Simulate the dialog was confirmed.
    editEntityDialog.dispatchEvent(
        new CustomEvent('autofill-ai-add-or-edit-done', {
          bubbles: true,
          composed: true,
          detail: testEntity,
        }));

    const editedEntity =
        await entityDataManager.whenCalled('addOrUpdateEntityInstance');
    assertDeepEquals(testEntity, editedEntity);
  });

  test('testEntriesDoNotDisappearAfterToggleDisabling', async function() {
    // The toggle is initially enabled (see the setup() method), clicking it
    // disables the 'autofill.prediction_improvements.enabled' pref.
    assertTrue(section.prefs.autofill.prediction_improvements.enabled.value);
    section.shadowRoot!.querySelector<HTMLElement>('#prefToggle')!.click();
    await flushTasks();
    assertFalse(section.prefs.autofill.prediction_improvements.enabled.value);

    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'With the toggle disabled, the entries should be visible');
  });
});
