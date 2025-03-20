// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {CrButtonElement, CrInputElement, SettingsAutofillAiAddOrEditDialogElement} from 'chrome://settings/lazy_load.js';
import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;

suite('AutofillAiAddOrEditDialogUiTest', function() {
  let dialog: SettingsAutofillAiAddOrEditDialogElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let testEntityInstance: chrome.autofillPrivate.EntityInstance;
  let testAttributeTypes: chrome.autofillPrivate.AttributeType[];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    testEntityInstance = {
      type: {
        typeName: 2,
        typeNameAsString: 'Vehicle',
        addEntityTypeString: 'Add vehicle',
        editEntityTypeString: 'Edit vehicle',
      },
      attributeInstances: [
        {
          type: {
            typeName: 10,
            typeNameAsString: 'Make',
            dataType: AttributeTypeDataType.STRING,
          },
          value: 'Toyota',
        },
        {
          type: {
            typeName: 13,
            typeNameAsString: 'Owner',
            dataType: AttributeTypeDataType.STRING,
          },
          value: 'Mark Nolan',
        },
        {
          type: {
            typeName: 16,
            typeNameAsString: 'VIN',
            dataType: AttributeTypeDataType.STRING,
          },
          value: 'ABCDE123',
        },
      ],
      guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
      nickname: 'My car',
    };
    testAttributeTypes = [
      {
        typeName: 10,
        typeNameAsString: 'Make',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 11,
        typeNameAsString: 'Model',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 12,
        typeNameAsString: 'Year',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 13,
        typeNameAsString: 'Owner',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 14,
        typeNameAsString: 'Plate number',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 15,
        typeNameAsString: 'Plate state',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 16,
        typeNameAsString: 'VIN',
        dataType: AttributeTypeDataType.STRING,
      },
    ];
    entityDataManager.setGetAllAttributeTypesForEntityTypeNameResponse(
        testAttributeTypes);

    dialog = document.createElement('settings-autofill-ai-add-or-edit-dialog');
  });

  interface AddOrEditParamsInterface {
    // Whether the add or edit dialog should be confirmed or cancelled.
    confirmed: boolean;
    // True if the user is adding an entity instance, false if the user is
    // editing an entity instance.
    add: boolean;
    // The title of the test.
    title: string;
  }

  const addOrEditEntityInstanceParams: AddOrEditParamsInterface[] = [
    {confirmed: true, add: true, title: 'testAddEntityInstanceConfirmed'},
    {confirmed: true, add: false, title: 'testEditEntityInstanceConfirmed'},
    {confirmed: false, add: true, title: 'testAddEntityInstanceCancelled'},
    {confirmed: false, add: false, title: 'testEditEntityInstanceCancelled'},
  ];

  addOrEditEntityInstanceParams.forEach(
      (params) => test(params.title, async function() {
        const newAttributeInstanceValue = 'BMW';
        let expectedEntityInstance: chrome.autofillPrivate.EntityInstance;

        // Populate the dialog's entity instance and title and set expectations.
        if (params.add) {
          expectedEntityInstance = {
            type: testEntityInstance.type,
            attributeInstances: [
              {
                type: {
                  typeName: 10,
                  typeNameAsString: 'Make',
                  dataType: AttributeTypeDataType.STRING,
                },
                value: newAttributeInstanceValue,
              },
            ],
            guid: '',
            nickname: '',
          };

          dialog.entityInstance = {
            type: testEntityInstance.type,
            attributeInstances: [],
            guid: '',
            nickname: '',
          };
          dialog.dialogTitle = testEntityInstance.type.addEntityTypeString;
        } else {
          expectedEntityInstance = structuredClone(testEntityInstance);
          expectedEntityInstance.attributeInstances[0]!.value =
              newAttributeInstanceValue;

          dialog.entityInstance = structuredClone(testEntityInstance);
          dialog.dialogTitle = testEntityInstance.type.editEntityTypeString;
        }
        document.body.appendChild(dialog);
        await entityDataManager.whenCalled(
            'getAllAttributeTypesForEntityTypeName');
        await flushTasks();

        // Verify that the dialog title is correct.
        const dialogTitle =
            dialog.shadowRoot!.querySelector<HTMLElement>('div[slot="title"]');
        assertTrue(
            dialogTitle!.textContent!.includes(
                params.add ? 'Add vehicle' : 'Edit vehicle'));

        // Edit first field.
        const firstAttributeInstanceField =
            dialog.shadowRoot!.querySelector<CrInputElement>(
                '#attributeInstanceField');
        assertTrue(!!firstAttributeInstanceField);
        firstAttributeInstanceField.value = newAttributeInstanceValue;
        await flushTasks();

        if (params.confirmed) {
          // Verify that the entity instance was changed.
          const saveButton =
              dialog.shadowRoot!.querySelector<HTMLElement>('.action-button');
          assertTrue(!!saveButton);

          const dialogConfirmedPromise =
              eventToPromise('autofill-ai-add-or-edit-done', dialog);
          saveButton.click();

          const dialogConfirmedEvent = await dialogConfirmedPromise;
          await flushTasks();

          assertFalse(dialog.$.dialog.getNative().open);
          assertDeepEquals(expectedEntityInstance, dialogConfirmedEvent.detail);
        } else {
          // Verify that the entity instance was not changed.
          const cancelButton =
              dialog.shadowRoot!.querySelector<HTMLElement>('.cancel-button');
          assertTrue(!!cancelButton);
          const dialogCancelledPromise =
              eventToPromise('cancel', dialog.$.dialog);
          cancelButton.click();

          await dialogCancelledPromise;
          assertFalse(dialog.$.dialog.getNative().open);
        }
      }));

  test('testAddOrEditEntityInstanceValidationError', async function() {
    dialog.entityInstance = testEntityInstance;
    document.body.appendChild(dialog);
    await entityDataManager.whenCalled('getAllAttributeTypesForEntityTypeName');
    await flushTasks();

    // The validation error should not be visible yet and the save button
    // should be enabled.
    const validationError =
        dialog.shadowRoot!.querySelector<HTMLElement>('#validationError');
    const saveButton =
        dialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!validationError);
    assertTrue(!!saveButton);
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);

    // Simulate that the user writes only one whitespace (' ') character in all
    // fields.
    const attributeInstanceFields =
        dialog.shadowRoot!.querySelectorAll<CrInputElement>(
            '#attributeInstanceField');
    assertTrue(!!attributeInstanceFields);
    attributeInstanceFields.forEach(
        (attributeInstanceField: CrInputElement) =>
            attributeInstanceField.value = ' ');
    await flushTasks();
    attributeInstanceFields[0]!.dispatchEvent(new Event('input'));
    await flushTasks();

    // All fields are empty, but the save button was not clicked yet, so there
    // is no validation error.
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);

    saveButton.click();
    await flushTasks();
    // All fields are empty and the save button was clicked, so the validation
    // error should be visible and the save button should be disabled.
    assertTrue(isVisible(validationError));
    assertTrue(saveButton.disabled);

    attributeInstanceFields[0]!.value = 'something';
    await flushTasks();
    attributeInstanceFields[0]!.dispatchEvent(new Event('input'));
    await flushTasks();

    // One field is not empty, so the validation error should not be visible
    // anymore and the save button should be enabled.
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);
  });
});

suite('AutofillAiAddOrEditDialogSelectElementUiTest', function() {
  let dialog: SettingsAutofillAiAddOrEditDialogElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let testEntityInstance: chrome.autofillPrivate.EntityInstance;
  let testAttributeTypes: chrome.autofillPrivate.AttributeType[];

  async function simulateCountryChange(
      countrySelect: HTMLSelectElement, newCountryCode: string) {
    countrySelect.value = newCountryCode;
    countrySelect.dispatchEvent(new CustomEvent('change'));
    await flushTasks();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    // For easier testing, all fields are empty except for the country.
    testEntityInstance = {
      type: {
        typeName: 1,
        typeNameAsString: 'Passport',
        addEntityTypeString: 'Add passport',
        editEntityTypeString: 'Edit passport',
      },
      attributeInstances: [
        {
          type: {
            typeName: 0,
            typeNameAsString: 'Name',
            dataType: AttributeTypeDataType.STRING,
          },
          value: '',
        },
        {
          type: {
            typeName: 1,
            typeNameAsString: 'Country',
            dataType: AttributeTypeDataType.COUNTRY,
          },
          value: 'Germany',
        },
      ],
      guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
      nickname: 'My passport',
    };
    testAttributeTypes = [
      {
        typeName: 0,
        typeNameAsString: 'Name',
        dataType: AttributeTypeDataType.STRING,
      },
      {
        typeName: 1,
        typeNameAsString: 'Country',
        dataType: AttributeTypeDataType.COUNTRY,
      },
    ];
    entityDataManager.setGetAllAttributeTypesForEntityTypeNameResponse(
        testAttributeTypes);

    dialog = document.createElement('settings-autofill-ai-add-or-edit-dialog');
  });

  interface AddOrEditEntityInstanceCountryParamsInterface {
    // True if the user is adding an entity instance, false if the user is
    // editing an entity instance.
    add: boolean;
    // If true, the country is changed. This param should always be true for
    // adding.
    changeCountry: boolean;
    // The title of the test.
    title: string;
  }

  const addOrEditEntityInstanceCountryParams:
      AddOrEditEntityInstanceCountryParamsInterface[] = [
        {add: true, changeCountry: true, title: 'testAddEntityInstanceCountry'},
        {
          add: false,
          changeCountry: true,
          title: 'testEditEntityInstanceCountry',
        },
        {
          add: false,
          changeCountry: false,
          title: 'testEditEntityInstanceDontChangeCountry',
        },
      ];

  addOrEditEntityInstanceCountryParams.forEach(
      (params) => test(params.title, async function() {
        // Set up the test.
        const oldCountryCode = 'DE';
        const newCountryCode = 'CH';
        if (params.add) {
          testEntityInstance = {
            type: testEntityInstance.type,
            attributeInstances: [],
            guid: '',
            nickname: '',
          };
        }
        dialog.entityInstance = structuredClone(testEntityInstance);
        document.body.appendChild(dialog);
        await entityDataManager.whenCalled(
            'getAllAttributeTypesForEntityTypeName');
        await flushTasks();

        // Retrieve the country selector.
        const countrySelect =
            dialog.shadowRoot!.querySelector<HTMLSelectElement>(
                '#country-select');
        assertTrue(!!countrySelect);
        if (params.add) {
          assertEquals('', countrySelect.value);
          assertTrue(countrySelect.textContent!.includes('No option selected'));
        } else {
          assertEquals(oldCountryCode, countrySelect.value);
          assertTrue(countrySelect.textContent!.includes('Germany'));
        }

        if (params.changeCountry) {
          await simulateCountryChange(countrySelect, newCountryCode);
        }

        // Confirm the dialog and verify that the new country code is saved.
        const saveButton =
            dialog.shadowRoot!.querySelector<HTMLElement>('.action-button');
        assertTrue(!!saveButton);

        const dialogConfirmedPromise =
            eventToPromise('autofill-ai-add-or-edit-done', dialog);
        saveButton.click();

        const dialogConfirmedEvent = await dialogConfirmedPromise;
        await flushTasks();

        const expectedEntityInstance = structuredClone(testEntityInstance);
        expectedEntityInstance.attributeInstances = [{
          type: {
            typeName: 1,
            typeNameAsString: 'Country',
            dataType: AttributeTypeDataType.COUNTRY,
          },
          value: params.changeCountry ? newCountryCode : oldCountryCode,
        }];

        assertDeepEquals(expectedEntityInstance, dialogConfirmedEvent.detail);
      }));

  test('testAddOrEditEntityInstanceCountryValidationError', async function() {
    dialog.entityInstance = testEntityInstance;
    document.body.appendChild(dialog);
    await entityDataManager.whenCalled('getAllAttributeTypesForEntityTypeName');
    await flushTasks();

    // The validation error should not be visible yet and the save button
    // should be enabled.
    const validationError =
        dialog.shadowRoot!.querySelector<HTMLElement>('#validationError');
    const saveButton =
        dialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!validationError);
    assertTrue(!!saveButton);
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);

    // Simulate that the user clears the country field.
    const countrySelect =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#country-select');
    assertTrue(!!countrySelect);
    await simulateCountryChange(countrySelect, '');

    // All fields are empty, but the save button was not clicked yet, so there
    // is no validation error.
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);

    saveButton.click();
    await flushTasks();
    // All fields are empty and the save button was clicked, so the validation
    // error should be visible and the save button should be disabled.
    assertTrue(isVisible(validationError));
    assertTrue(saveButton.disabled);

    await simulateCountryChange(countrySelect, 'DE');

    // One field is not empty, so the validation error should not be visible
    // anymore and the save button should be enabled.
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);
  });
});
