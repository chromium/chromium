// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {CrButtonElement, CrInputElement, SettingsAutofillAiAddOrEditDialogElement} from 'chrome://settings/lazy_load.js';
import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;
type DateValue = chrome.autofillPrivate.DateValue;

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
        deleteEntityTypeString: 'Delete vehicle',
        supportsWalletStorage: false,
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
    {confirmed: true, add: true, title: 'AddEntityInstanceConfirmed'},
    {confirmed: true, add: false, title: 'EditEntityInstanceConfirmed'},
    {confirmed: false, add: true, title: 'AddEntityInstanceCancelled'},
    {confirmed: false, add: false, title: 'EditEntityInstanceCancelled'},
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
            dialogTitle!.textContent.includes(
                params.add ? 'Add vehicle' : 'Edit vehicle'));

        // Edit first field.
        const firstAttributeInstanceField =
            dialog.shadowRoot!.querySelector<CrInputElement>(
                '#attribute-instance-field');
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

  test('AddOrEditEntityInstanceValidationError', async function() {
    dialog.entityInstance = testEntityInstance;
    document.body.appendChild(dialog);
    await entityDataManager.whenCalled('getAllAttributeTypesForEntityTypeName');
    await flushTasks();

    // The validation error should not be visible yet and the save button
    // should be enabled.
    const validationError =
        dialog.shadowRoot!.querySelector<HTMLElement>('#validation-error');
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
            '#attribute-instance-field');
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
  let testCountryAttributeInstance: chrome.autofillPrivate.AttributeInstance;
  let testDateAttributeInstance: chrome.autofillPrivate.AttributeInstance;

  async function simulateCountryChange(
      countrySelect: HTMLSelectElement, newCountryCode: string) {
    countrySelect.value = newCountryCode;
    countrySelect.dispatchEvent(new CustomEvent('change'));
    await flushTasks();
  }

  function simulateSelectChange(select: HTMLSelectElement, value: string) {
    select.value = value;
    select.dispatchEvent(new CustomEvent('change'));
  }

  function simulateDateChange(
      monthSelect: HTMLSelectElement, daySelect: HTMLSelectElement,
      yearSelect: HTMLSelectElement, newDate: DateValue) {
    simulateSelectChange(monthSelect, newDate.month);
    simulateSelectChange(daySelect, newDate.day);
    simulateSelectChange(yearSelect, newDate.year);
    return flushTasks();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.documentElement.lang = 'en';

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    testCountryAttributeInstance = {
      type: {
        typeName: 1,
        typeNameAsString: 'Country',
        dataType: AttributeTypeDataType.COUNTRY,
      },
      value: 'Germany',
    };
    testDateAttributeInstance = {
      type: {
        typeName: 2,
        typeNameAsString: 'Issue date',
        dataType: AttributeTypeDataType.DATE,
      },
      value: {
        month: '5',
        day: '20',
        year: '2025',
      },
    };

    // For easier testing, the test entity instance has no attributes. The tests
    // will populate the attributes they need.
    testEntityInstance = {
      type: {
        typeName: 1,
        typeNameAsString: 'Passport',
        addEntityTypeString: 'Add passport',
        editEntityTypeString: 'Edit passport',
        deleteEntityTypeString: 'Delete passport',
        supportsWalletStorage: false,
      },
      attributeInstances: [],
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
      {
        typeName: 2,
        typeNameAsString: 'Issue date',
        dataType: AttributeTypeDataType.DATE,
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
        {add: true, changeCountry: true, title: 'AddEntityInstanceCountry'},
        {
          add: false,
          changeCountry: true,
          title: 'EditEntityInstanceCountry',
        },
        {
          add: false,
          changeCountry: false,
          title: 'EditEntityInstanceDontChangeCountry',
        },
      ];

  addOrEditEntityInstanceCountryParams.forEach(
      (params) => test(params.title, async function() {
        // Set up the test.
        const oldCountryCode = 'DE';
        const newCountryCode = 'CH';
        if (!params.add) {
          testEntityInstance.attributeInstances.push(
              testCountryAttributeInstance);
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
          assertTrue(countrySelect.textContent.includes('Select'));
        } else {
          assertEquals(oldCountryCode, countrySelect.value);
          assertTrue(countrySelect.textContent.includes('Germany'));
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

  interface AddOrEditEntityInstanceDateParamsInterface {
    // True if the user is adding an entity instance, false if the user is
    // editing an entity instance.
    add: boolean;
    // If true, the date is changed. This param should always be true for
    // adding.
    changeDate: boolean;
    // The title of the test.
    title: string;
  }

  const addOrEditEntityInstanceDateParams:
      AddOrEditEntityInstanceDateParamsInterface[] = [
        {add: true, changeDate: true, title: 'AddEntityInstanceDate'},
        {
          add: false,
          changeDate: true,
          title: 'EditEntityInstanceDate',
        },
        {
          add: false,
          changeDate: false,
          title: 'EditEntityInstanceDontChangeDate',
        },
      ];

  addOrEditEntityInstanceDateParams.forEach(
      (params) => test(params.title, async function() {
        // Set up the test.
        const oldDate: DateValue = testDateAttributeInstance.value as DateValue;
        const newDate: DateValue = {
          month: '3',
          day: '25',
          year: '2024',
        };
        if (!params.add) {
          testEntityInstance.attributeInstances.push(testDateAttributeInstance);
        }
        dialog.entityInstance = structuredClone(testEntityInstance);
        document.body.appendChild(dialog);
        await entityDataManager.whenCalled(
            'getAllAttributeTypesForEntityTypeName');
        await flushTasks();

        // Retrieve the date selectors.
        const monthSelect = dialog.shadowRoot!.querySelector<HTMLSelectElement>(
            '#month-select');
        const daySelect =
            dialog.shadowRoot!.querySelector<HTMLSelectElement>('#day-select');
        const yearSelect =
            dialog.shadowRoot!.querySelector<HTMLSelectElement>('#year-select');
        assertTrue(!!monthSelect);
        assertTrue(!!daySelect);
        assertTrue(!!yearSelect);

        if (params.add) {
          assertEquals('', monthSelect.value);
          assertEquals('', daySelect.value);
          assertEquals('', yearSelect.value);
          assertTrue(monthSelect.textContent.includes('MM'));
          assertTrue(daySelect.textContent.includes('DD'));
          assertTrue(yearSelect.textContent.includes('YYYY'));
        } else {
          assertEquals(oldDate.month, monthSelect.value);
          assertEquals(oldDate.day, daySelect.value);
          assertEquals(oldDate.year, yearSelect.value);
          assertTrue(monthSelect.textContent.includes('Mar'));
          assertTrue(daySelect.textContent.includes(oldDate.day));
          assertTrue(yearSelect.textContent.includes(oldDate.year));
        }

        if (params.changeDate) {
          await simulateDateChange(monthSelect, daySelect, yearSelect, newDate);
        }

        // Confirm the dialog and verify that the new date is saved.
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
            typeName: 2,
            typeNameAsString: 'Issue date',
            dataType: AttributeTypeDataType.DATE,
          },
          value: params.changeDate ? newDate : oldDate,
        }];

        assertDeepEquals(expectedEntityInstance, dialogConfirmedEvent.detail);
      }));

  test('EditEntityInstanceExistingYearOutOfBounds', async function() {
    // Set up the test.
    (testDateAttributeInstance.value as DateValue).year = '1800';
    testEntityInstance.attributeInstances.push(testDateAttributeInstance);
    dialog.entityInstance = structuredClone(testEntityInstance);
    document.body.appendChild(dialog);
    await entityDataManager.whenCalled('getAllAttributeTypesForEntityTypeName');
    await flushTasks();

    // Retrieve the year selector.
    const yearSelect =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#year-select');
    assertTrue(!!yearSelect);

    // Check that the out of bounds year is pre-selected.
    assertEquals('1800', yearSelect.value);
    // Simulate that changing to a year that is in bounds works.
    simulateSelectChange(yearSelect, '2000');
    await flushTasks();
    assertEquals('2000', yearSelect.value);

    // Simulate that changing back to the already existing year (that is out of
    // bounds) works.
    simulateSelectChange(yearSelect, '1800');
    await flushTasks();
    assertEquals('1800', yearSelect.value);
  });

  test('AddOrEditEntityInstanceCountryValidationError', async function() {
    testEntityInstance.attributeInstances.push(testCountryAttributeInstance);
    dialog.entityInstance = testEntityInstance;
    document.body.appendChild(dialog);
    await entityDataManager.whenCalled('getAllAttributeTypesForEntityTypeName');
    await flushTasks();

    // The validation error should not be visible yet and the save button
    // should be enabled.
    const validationError =
        dialog.shadowRoot!.querySelector<HTMLElement>('#validation-error');
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

  test('AddOrEditEntityInstanceDateValidationError', async function() {
    testEntityInstance.attributeInstances.push(testDateAttributeInstance);
    dialog.entityInstance = testEntityInstance;
    document.body.appendChild(dialog);
    await entityDataManager.whenCalled('getAllAttributeTypesForEntityTypeName');
    await flushTasks();

    // The invalid label and validation errors should not be visible yet, and
    // the save button should be enabled.
    const dateSelectLabel =
        dialog.shadowRoot!.querySelector<HTMLElement>('#date-select-label');
    const invalidDateSelectLabel =
        dialog.shadowRoot!.querySelector<HTMLElement>(
            '#invalid-date-select-label');
    const dateValidationError =
        dialog.shadowRoot!.querySelector<HTMLElement>('#date-validation-error');
    const regularValidationError =
        dialog.shadowRoot!.querySelector<HTMLElement>('#validation-error');
    const saveButton =
        dialog.shadowRoot!.querySelector<CrButtonElement>('.action-button');
    assertTrue(!!dateSelectLabel);
    assertTrue(!!invalidDateSelectLabel);
    assertTrue(!!dateValidationError);
    assertTrue(!!saveButton);
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertFalse(saveButton.disabled);

    // Simulate that the user introduces an invalid date (30th of February).
    const invalidDate = {month: '2', day: '30', year: '2022'};
    const monthSelect =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#month-select');
    const daySelect =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#day-select');
    const yearSelect =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('#year-select');
    assertTrue(!!monthSelect);
    assertTrue(!!daySelect);
    assertTrue(!!yearSelect);
    await simulateDateChange(monthSelect, daySelect, yearSelect, invalidDate);

    // The date is invalid, but the save button was not clicked yet, so there
    // is no validation error.
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertFalse(saveButton.disabled);

    // Simulate that the user clears the date field.
    await simulateDateChange(
        monthSelect, daySelect, yearSelect, {month: '', day: '', year: ''});

    // All fields are empty, but the save button was not clicked yet, so there
    // is no validation error.
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertFalse(saveButton.disabled);

    saveButton.click();
    await flushTasks();

    // All fields are empty and the save button was clicked, so the regular
    // validation error should be visible and the save button should be
    // disabled. The date validation error and the invalid select label should
    // not be visible.
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertTrue(isVisible(regularValidationError));
    assertTrue(saveButton.disabled);

    await simulateDateChange(
        monthSelect, daySelect, yearSelect,
        {month: '3', day: '17', year: '2024'});

    // One field is not empty, so no validation errors
    // should be visible anymore and the save button should be enabled.
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertFalse(saveButton.disabled);

    // Populate the name field and completely clear the date field.
    const nameField = dialog.shadowRoot!.querySelector<CrInputElement>(
        '#attribute-instance-field');
    assertTrue(!!nameField);
    nameField.value = 'John Doe';
    await flushTasks();
    await simulateDateChange(
        monthSelect, daySelect, yearSelect, {month: '', day: '', year: ''});

    // One field is not empty, so no validation errors
    // should be visible and the save button should be enabled.
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertFalse(saveButton.disabled);

    await simulateDateChange(
        monthSelect, daySelect, yearSelect,
        {month: '2', day: '', year: '2020'});

    // The date is incomplete, so the invalid label and the date validation
    // error should be visible and the save button should be disabled. The
    // regular validation error should not be visible.
    assertFalse(isVisible(dateSelectLabel));
    assertTrue(isVisible(invalidDateSelectLabel));
    assertTrue(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertTrue(saveButton.disabled);

    await simulateDateChange(
        monthSelect, daySelect, yearSelect,
        {month: '2', day: '', year: '2020'});

    // The date is incomplete, so the invalid label and the date validation
    // error should be visible and the save button should be disabled. The
    // regular validation error should not be visible.
    assertFalse(isVisible(dateSelectLabel));
    assertTrue(isVisible(invalidDateSelectLabel));
    assertTrue(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertTrue(saveButton.disabled);

    await simulateDateChange(
        monthSelect, daySelect, yearSelect,
        {month: '9', day: '5', year: '2022'});

    // The date is now valid, so the invalid label and the date validation error
    // should not be visible and the save button should be enabled. The regular
    // validation error should not be visible.
    assertTrue(isVisible(dateSelectLabel));
    assertFalse(isVisible(invalidDateSelectLabel));
    assertFalse(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertFalse(saveButton.disabled);

    await simulateDateChange(monthSelect, daySelect, yearSelect, invalidDate);

    // The date is now invalid, so the invalid label and the date validation
    // error should be visible and the save button should be disabled. The
    // regular validation error should not be visible.
    assertFalse(isVisible(dateSelectLabel));
    assertTrue(isVisible(invalidDateSelectLabel));
    assertTrue(isVisible(dateValidationError));
    assertFalse(isVisible(regularValidationError));
    assertTrue(saveButton.disabled);
  });

  interface MonthPickerHasCorrectMonthsParamsInterface {
    // The locale for which the test should take place.
    locale: string;
    // The months abbreviation in the specified locale.
    monthsAbbreviations: string[];
    // The title of the test.
    title: string;
  }

  // The months abbreviations are formatted so that they match the way they are
  // found inside the `textContent` of <option> elements. This way, "led" will
  // not match "New Caledonia" from the country picker.
  const monthPickerHasCorrectMonthsParams:
      MonthPickerHasCorrectMonthsParamsInterface[] = [
        {
          locale: 'en',
          monthsAbbreviations: [
            ' Jan\n',
            ' Feb\n',
            ' Mar\n',
            ' Apr\n',
            ' May\n',
            ' Jun\n',
            ' Jul\n',
            ' Aug\n',
            ' Sep\n',
            ' Oct\n',
            ' Nov\n',
            ' Dec\n',
          ],
          title: 'MonthPickerHasCorrectMonthsEnglishLocale',
        },
        {
          locale: 'cs',
          monthsAbbreviations: [
            ' led\n',
            ' úno\n',
            ' bře\n',
            ' dub\n',
            ' kvě\n',
            ' čvn\n',
            ' čvc\n',
            ' srp\n',
            ' zář\n',
            ' říj\n',
            ' lis\n',
            ' pro\n',
          ],
          title: 'MonthPickerHasCorrectMonthsCzechLocale',
        },
      ];

  monthPickerHasCorrectMonthsParams.forEach(
      (params) => test(params.title, async function() {
        testEntityInstance.attributeInstances.push(testDateAttributeInstance);
        dialog.entityInstance = testEntityInstance;
        document.documentElement.lang = params.locale;
        document.body.appendChild(dialog);
        await entityDataManager.whenCalled(
            'getAllAttributeTypesForEntityTypeName');
        await flushTasks();

        const allSelectorOptions =
            dialog.shadowRoot!.querySelectorAll<HTMLElement>('option');
        const firstOptionInTheMonthSelectorIndex =
            Array.from(allSelectorOptions)
                .findIndex(
                    option => option.textContent.includes(
                        params.monthsAbbreviations[0]!));
        assertNotEquals(firstOptionInTheMonthSelectorIndex, -1);

        for (let i = 0, j = firstOptionInTheMonthSelectorIndex;
             i < params.monthsAbbreviations.length; i++, j++) {
          assertTrue(allSelectorOptions.item(j).textContent.includes(
              params.monthsAbbreviations[i]!));
        }
      }));
});
