// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {CrButtonElement, CrInputElement, SettingsAutofillAiAddOrEditDialogElement} from 'chrome://settings/lazy_load.js';
import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

suite('AutofillAiAddOrEditDialogUiTest', function() {
  let dialog: SettingsAutofillAiAddOrEditDialogElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let testEntity: chrome.autofillPrivate.EntityInstance;
  let testAttributeTypes: chrome.autofillPrivate.AttributeType[];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    testEntity = {
      type: {
        typeName: 2,
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
    testAttributeTypes = [
      {
        typeName: 8,
        typeNameAsString: 'Owner',
      },
      {
        typeName: 9,
        typeNameAsString: 'License plate',
      },
      {
        typeName: 10,
        typeNameAsString: 'Registration',
      },
      {
        typeName: 11,
        typeNameAsString: 'Make',
      },
      {
        typeName: 12,
        typeNameAsString: 'Model',
      },
    ];
    entityDataManager.setGetAllAttributeTypesForEntityResponse(
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

  const addOrEditEntityParams: AddOrEditParamsInterface[] = [
    {confirmed: true, add: true, title: 'testAddEntityConfirmed'},
    {confirmed: true, add: false, title: 'testEditEntityConfirmed'},
    {confirmed: false, add: true, title: 'testAddEntityCancelled'},
    {confirmed: false, add: false, title: 'testEditEntityCancelled'},
  ];

  addOrEditEntityParams.forEach(
      (params) => test(params.title, async function() {
        const newAttributeValue = 'John Steven';
        let expectedEntity: chrome.autofillPrivate.EntityInstance;

        // Populate the dialog's entity and title and set expectations.
        if (params.add) {
          expectedEntity = {
            type: testEntity.type,
            attributes: [
              {
                type: {
                  typeName: 8,
                  typeNameAsString: 'Owner',
                },
                value: newAttributeValue,
              },
            ],
            guid: '',
            nickname: '',
          };

          dialog.entity = {
            type: testEntity.type,
            attributes: [],
            guid: '',
            nickname: '',
          };
          dialog.dialogTitle = testEntity.type.addEntityString;
        } else {
          expectedEntity = structuredClone(testEntity);
          expectedEntity.attributes[0]!.value = newAttributeValue;

          dialog.entity = structuredClone(testEntity);
          dialog.dialogTitle = testEntity.type.editEntityString;
        }
        document.body.appendChild(dialog);
        await flushTasks();

        // Verify that the dialog title is correct.
        const dialogTitle =
            dialog.shadowRoot!.querySelector<HTMLElement>('div[slot="title"]');
        assertTrue(
            dialogTitle!.textContent!.includes(
                params.add ? 'Add car' : 'Edit car'));

        // Edit first field.
        const firstAttributeField =
            dialog.shadowRoot!.querySelector<CrInputElement>('#attributeField');
        assertTrue(!!firstAttributeField);
        firstAttributeField.value = newAttributeValue;
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
          assertDeepEquals(expectedEntity, dialogConfirmedEvent.detail);
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

  test('testAddOrEditEntityValidationError', async function() {
    dialog.entity = testEntity;
    document.body.appendChild(dialog);
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
    const attributeFields =
        dialog.shadowRoot!.querySelectorAll<CrInputElement>('#attributeField');
    assertTrue(!!attributeFields);
    attributeFields.forEach(
        (attributeField: CrInputElement) => attributeField.value = ' ');
    await flushTasks();
    attributeFields[0]!.dispatchEvent(new Event('input'));
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

    attributeFields[0]!.value = 'something';
    await flushTasks();
    attributeFields[0]!.dispatchEvent(new Event('input'));
    await flushTasks();

    // One field is not empty, so the validation error should not be visible
    // anymore and the save button should be enabled.
    assertFalse(isVisible(validationError));
    assertFalse(saveButton.disabled);
  });
});
