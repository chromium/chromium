// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsAiLoggingInfoBullet, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import type {CrButtonElement, SettingsAutofillAiAddOrEditDialogElement, SettingsSimpleConfirmationDialogElement, SettingsAutofillAiSectionElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;

suite('AutofillAiSectionUiReflectsEligibilityStatus', function() {
  let section: SettingsAutofillAiSectionElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    // The tests need to simulate that the user has some entity instances saved,
    // because an ineligible user without any entity instances saved cannot see
    // the Autofill with Ai page.
    const testEntityInstancesWithLabels:
        chrome.autofillPrivate.EntityInstanceWithLabels[] = [
      {
        guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
        entityInstanceLabel: 'Toyota',
        entityInstanceSubLabel: 'Car',
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Passport',
      },
    ];
    entityDataManager.setLoadEntityInstancesResponse(
        testEntityInstancesWithLabels);
    // By default, the user is not opted in.
    entityDataManager.setGetOptInStatusResponse(false);

    section = document.createElement('settings-autofill-ai-section');
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    section.prefs = settingsPrefs.prefs;
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });
  interface EligibilityParamsInterface {
    // Whether the user is opted into Autofill with Ai.
    optedIn: boolean;
    // Whether the user is eligible for Autofill with Ai.
    ineligibleUser: boolean;
    // The title of the test.
    title: string;
  }

  const eligibilityParams: EligibilityParamsInterface[] = [
    {optedIn: true, ineligibleUser: true, title: 'testOptedInIneligibleUser'},
    {optedIn: true, ineligibleUser: false, title: 'testOptedInEligibleUser'},
    {optedIn: false, ineligibleUser: true, title: 'testOptedOutIneligibleUser'},
    {optedIn: false, ineligibleUser: false, title: 'testOptedOutEligibleUser'},
  ];

  eligibilityParams.forEach(
      (params) => test(params.title, async function() {
        section.ineligibleUser = params.ineligibleUser;
        entityDataManager.setGetOptInStatusResponse(params.optedIn);

        document.body.appendChild(section);
        await flushTasks();

        const toggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#prefToggle');
        assertTrue(!!toggle);
        assertEquals(toggle.disabled, params.ineligibleUser);
        assertEquals(toggle.checked, !params.ineligibleUser && params.optedIn);

        const addButton = section.shadowRoot!.querySelector<CrButtonElement>(
            '#addEntityInstance');
        assertTrue(!!addButton);
        assertEquals(
            addButton.disabled, params.ineligibleUser || !params.optedIn);

        assertTrue(
            isVisible(section.shadowRoot!.querySelector('#entries')),
            'The entries should always be visible');
      }));

  test('testSwitchingToggleUpdatesPref', async function() {
    // The user is eligible so that the toggle is enabled.
    section.ineligibleUser = false;
    document.body.appendChild(section);
    await flushTasks();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#prefToggle');
    assertTrue(!!toggle);

    toggle.click();
    assertTrue(await entityDataManager.whenCalled('setOptInStatus'));
    entityDataManager.reset();
    await flushTasks();

    toggle.click();
    assertFalse(await entityDataManager.whenCalled('setOptInStatus'));
  });
});

suite('AutofillAiSectionUiTest', function() {
  let section: SettingsAutofillAiSectionElement;
  let entityInstancesListElement: HTMLElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let testEntityInstance: chrome.autofillPrivate.EntityInstance;
  let testEntityTypes: chrome.autofillPrivate.EntityType[];
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    testEntityInstance = {
      type: {
        typeName: 1,
        typeNameAsString: 'Driver\'s license',
        addEntityTypeString: 'Add driver\'s license',
        editEntityTypeString: 'Edit driver\'s license',
        deleteEntityTypeString: 'Delete driver\'s license',
      },
      attributeInstances: [
        {
          type: {
            typeName: 5,
            typeNameAsString: 'Name',
            dataType: AttributeTypeDataType.STRING,
          },
          value: 'John Doe',
        },
        {
          type: {
            typeName: 7,
            typeNameAsString: 'Number',
            dataType: AttributeTypeDataType.STRING,
          },
          value: 'ABCDE123',
        },
      ],
      guid: 'd70b5bb7-49a6-4276-b4b7-b014dacdc9e6',
      nickname: 'My license',
    };
    // Initially not sorted alphabetically. The production code should sort them
    // alphabetically.
    testEntityTypes = [
      {
        typeName: 0,
        typeNameAsString: 'Passport',
        addEntityTypeString: 'Add passport',
        editEntityTypeString: 'Edit passport',
        deleteEntityTypeString: 'Delete passport',
      },
      {
        typeName: 2,
        typeNameAsString: 'Car',
        addEntityTypeString: 'Add car',
        editEntityTypeString: 'Edit car',
        deleteEntityTypeString: 'Delete car',
      },
    ];
    // Initially not sorted alphabetically. The production code should sort them
    // alphabetically.
    const testEntityInstancesWithLabels:
        chrome.autofillPrivate.EntityInstanceWithLabels[] = [
      {
        guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
        entityInstanceLabel: 'Toyota',
        entityInstanceSubLabel: 'Car',
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Passport',
      },
      {
        guid: 'd70b5bb7-49a6-4276-b4b7-b014dacdc9e6',
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Driver\'s license',
      },
    ];
    entityDataManager.setGetOptInStatusResponse(true);
    entityDataManager.setGetAllEntityTypesResponse(
        structuredClone(testEntityTypes));
    entityDataManager.setLoadEntityInstancesResponse(
        testEntityInstancesWithLabels);

    // `testEntityTypes` now contains expected values, so they should be sorted
    // alphabetically.
    testEntityTypes.sort(
        (a, b) => a.typeNameAsString.localeCompare(b.typeNameAsString));
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  async function createPage() {
    section = document.createElement('settings-autofill-ai-section');
    section.prefs = settingsPrefs.prefs;
    document.body.appendChild(section);
    await flushTasks();

    const entityInstancesQueried =
        section.shadowRoot!.querySelector<HTMLElement>('#entries');
    assertTrue(!!entityInstancesQueried);
    entityInstancesListElement = entityInstancesQueried;

    assertTrue(!!section.shadowRoot!.querySelector('#entriesHeader'));
  }

  test(
      'testAutofillAiEnterpriseUserLoggingAllowedAndNonEnterpriseUserHaveNoLoggingInfoBullet',
      async function() {
        // Both enterprise and non enterprise users have the pref set to 0
        // (allow).
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.ALLOW);
        await createPage();

        const enterpriseLogginInfoBullet =
            section.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertFalse(!!enterpriseLogginInfoBullet);
      });

  test(
      'testAutofillAiEnterpriseUserLoggingNotAllowedHaveLoggingInfoBullet',
      async function() {
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
        await createPage();

        const enterpriseLogginInfoBullet =
            section.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertTrue(!!enterpriseLogginInfoBullet);
        assertEquals(
            loadTimeData.getString(
                'autofillAiSubpageSublabelLoggingManagedDisabled'),
            enterpriseLogginInfoBullet.loggingManagedDisabledCustomLabel);
      });

  test(
      'testAutofillAiEnterpriseUserDisabledHasLoggingInfoBullet',
      async function() {
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.DISABLE);
        await createPage();

        const enterpriseLogginInfoBullet =
            section.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertTrue(!!enterpriseLogginInfoBullet);
        assertEquals(
            loadTimeData.getString(
                'autofillAiSubpageSublabelLoggingManagedDisabled'),
            enterpriseLogginInfoBullet.loggingManagedDisabledCustomLabel);
      });

  test('testEntityInstancesLoadedAndSortedAlphabetically', async function() {
    await createPage();
    await entityDataManager.whenCalled('loadEntityInstances');
    const listItems =
        entityInstancesListElement.querySelectorAll<HTMLElement>('.list-item');

    assertEquals(
        4, listItems.length,
        '3 entity instances and a hidden element were loaded.');
    // The items should now also be sorted alphabetically.
    assertTrue(listItems[0]!.textContent!.includes('John Doe'));
    assertTrue(listItems[0]!.textContent!.includes('Driver\'s license'));
    assertTrue(listItems[1]!.textContent!.includes('John Doe'));
    assertTrue(listItems[1]!.textContent!.includes('Passport'));
    assertTrue(listItems[2]!.textContent!.includes('Toyota'));
    assertTrue(listItems[2]!.textContent!.includes('Car'));
    assertFalse(isVisible(listItems[3]!));
  });

  interface RemoveEntityInstanceParamsInterface {
    // Whether the user confirms the delete dialog.
    confirmed: boolean;
    // The title of the test.
    title: string;
  }

  const removeEntityInstanceParams: RemoveEntityInstanceParamsInterface[] = [
    {confirmed: true, title: 'testRemoveEntityInstanceConfirmed'},
    {confirmed: false, title: 'testRemoveEntityInstanceCancelled'},
  ];

  removeEntityInstanceParams.forEach(
      (params) => test(params.title, async function() {
        await createPage();
        entityDataManager.setGetEntityInstanceByGuidResponse(
            testEntityInstance);

        const actionMenuButton =
            entityInstancesListElement.querySelector<HTMLElement>(
                '#moreButton');
        assertTrue(!!actionMenuButton);
        actionMenuButton.click();
        await flushTasks();

        const deleteButton = section.shadowRoot!.querySelector<HTMLElement>(
            '#menuRemoveEntityInstance');

        assertTrue(!!deleteButton);
        deleteButton.click();
        await flushTasks();

        const removeEntityInstanceDialog =
            section.shadowRoot!
                .querySelector<SettingsSimpleConfirmationDialogElement>(
                    '#removeEntityInstanceDialog');
        assertTrue(!!removeEntityInstanceDialog);
        assertEquals(
            'Delete driver\'s license', removeEntityInstanceDialog.titleText);

        if (params.confirmed) {
          removeEntityInstanceDialog.$.confirm.click();
          const guid =
              await entityDataManager.whenCalled('removeEntityInstance');
          await flushTasks();

          assertEquals(
              1, entityDataManager.getCallCount('removeEntityInstance'));
          assertEquals('d70b5bb7-49a6-4276-b4b7-b014dacdc9e6', guid);
        } else {
          removeEntityInstanceDialog.$.cancel.click();
          await flushTasks();

          assertEquals(
              0, entityDataManager.getCallCount('removeEntityInstance'));
        }
      }));

  interface AddOrEditDialogParamsInterface {
    // True if the user is adding an entity instance, false if the user is
    // editing an entity instance.
    add: boolean;
    // The title of the test.
    title: string;
  }

  const addOrEditEntityInstanceDialogParams: AddOrEditDialogParamsInterface[] =
      [
        {add: true, title: 'testAddEntityInstanceDialogOpenAndConfirm'},
        {add: false, title: 'testEditEntityInstanceDialogOpenAndConfirm'},
      ];

  addOrEditEntityInstanceDialogParams.forEach(
      (params) => test(params.title, async function() {
        await createPage();
        if (params.add) {
          // Open the add entity instance dialog.
          const addButton = section.shadowRoot!.querySelector<HTMLElement>(
              '#addEntityInstance');
          assertTrue(!!addButton);
          addButton.click();
          await flushTasks();

          const addSpecificEntityTypeButton =
              section.shadowRoot!.querySelector<HTMLElement>(
                  '#addSpecificEntityType');
          assertTrue(!!addSpecificEntityTypeButton);
          addSpecificEntityTypeButton.click();
          await flushTasks();
        } else {
          // Open the edit entity instance dialog.
          entityDataManager.setGetEntityInstanceByGuidResponse(
              testEntityInstance);

          const actionMenuButton =
              entityInstancesListElement.querySelector<HTMLElement>(
                  '#moreButton');
          assertTrue(!!actionMenuButton);
          actionMenuButton.click();
          await flushTasks();

          const editButton = section.shadowRoot!.querySelector<HTMLElement>(
              '#menuEditEntityInstance');

          assertTrue(!!editButton);
          editButton.click();
          await flushTasks();
        }

        // Check that the dialog is populated with the correct entity instance
        // information.
        const addOrEditEntityInstanceDialog =
            section.shadowRoot!
                .querySelector<SettingsAutofillAiAddOrEditDialogElement>(
                    '#addOrEditEntityInstanceDialog');
        assertTrue(!!addOrEditEntityInstanceDialog);
        if (params.add) {
          assertDeepEquals(
              testEntityTypes[0],
              addOrEditEntityInstanceDialog.entityInstance!.type);
          assertEquals(
              0,
              addOrEditEntityInstanceDialog.entityInstance!.attributeInstances
                  .length);
          await flushTasks();
        } else {
          assertDeepEquals(
              testEntityInstance, addOrEditEntityInstanceDialog.entityInstance);
        }

        // Simulate the dialog was confirmed.
        addOrEditEntityInstanceDialog.dispatchEvent(
            new CustomEvent('autofill-ai-add-or-edit-done', {
              bubbles: true,
              composed: true,
              detail: testEntityInstance,
            }));

        const addedOrEditedEntityInstance =
            await entityDataManager.whenCalled('addOrUpdateEntityInstance');
        assertDeepEquals(testEntityInstance, addedOrEditedEntityInstance);
      }));

  test('testAddButtonShowsEntityInstancesList', async function() {
    await createPage();
    const addButton =
        section.shadowRoot!.querySelector<HTMLElement>('#addEntityInstance');
    assertTrue(!!addButton);
    addButton.click();
    await flushTasks();

    const addSpecificEntityTypeButtons =
        section.shadowRoot!.querySelectorAll<HTMLElement>(
            '#addSpecificEntityType');
    assertEquals(testEntityTypes.length, addSpecificEntityTypeButtons.length);
    for (const index in testEntityTypes) {
      assertTrue(
          addSpecificEntityTypeButtons[index]!.textContent!.includes(
              testEntityTypes[index]!.typeNameAsString));
    }
  });

  test(
      'testEntityInstancesChangedListenerUpdatesAndAlphabeticallySortsEntries',
      async function() {
        await createPage();
        const newTestEntityInstancesWithLabels:
            chrome.autofillPrivate.EntityInstanceWithLabels[] = [
          {
            guid: 'a521fc41-d672-4947-ab39-8bc9d49b08d2',
            entityInstanceLabel: 'Tom Clark',
            entityInstanceSubLabel: 'Passport',
          },
          {
            guid: 'db56681d-9598-4e37-825c-7977f52fbcee',
            entityInstanceLabel: 'Honda',
            entityInstanceSubLabel: 'Car',
          },
          {
            guid: '1a89869f-dff2-461a-8ef8-769e0e1c66f7',
            entityInstanceLabel: 'Tom Clark',
            entityInstanceSubLabel: 'Driver\'s license',
          },
        ];

        entityDataManager.callEntityInstancesChangedListener(
            newTestEntityInstancesWithLabels);
        await flushTasks();

        const listItems =
            entityInstancesListElement.querySelectorAll<HTMLElement>(
                '.list-item');
        assertEquals(
            4, listItems.length,
            'Three entity instances and a hidden element should be present.');
        // The items should now also be sorted alphabetically.
        assertTrue(listItems[0]!.textContent!.includes('Honda'));
        assertTrue(listItems[0]!.textContent!.includes('Car'));
        assertTrue(listItems[1]!.textContent!.includes('Tom Clark'));
        assertTrue(listItems[1]!.textContent!.includes('Driver\'s license'));
        assertTrue(listItems[2]!.textContent!.includes('Tom Clark'));
        assertTrue(listItems[2]!.textContent!.includes('Passport'));
        assertFalse(isVisible(listItems[3]!));
      });

  test('testEntriesDoNotDisappearAfterToggleDisabling', async function() {
    await createPage();
    // The toggle is initially enabled (see the setup() method). Clicking it
    // sets the opt-in status to false.
    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#prefToggle');
    assertTrue(!!toggle);
    assertFalse(toggle.disabled);
    toggle.click();

    assertFalse(await entityDataManager.whenCalled('setOptInStatus'));
    await flushTasks();

    assertTrue(
        isVisible(section.shadowRoot!.querySelector('#entries')),
        'With the toggle disabled, the entries should be visible');
  });

  test('testToggleIsDisabledWhenUserIsNotEligible', async function() {
    await createPage();
    // The toggle is initially enabled (see the setup() method). Clicking it
    // sets the opt-in status to false.
    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#prefToggle');
    assertTrue(!!toggle);
    assertFalse(toggle.disabled);
    assertTrue(toggle.checked);

    // Simulate a toggle click that fails because the user meanwhile became
    // ineligible for Autofill AI.
    entityDataManager.setSetOptInStatusResponse(false);
    toggle.click();

    assertFalse(await entityDataManager.whenCalled('setOptInStatus'));
    await flushTasks();

    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);
  });
});

suite('AutofillAiSectionLongLabelsUiTest', function() {
  let section: SettingsAutofillAiSectionElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    const testEntityInstancesWithLabels:
        chrome.autofillPrivate.EntityInstanceWithLabels[] = [
      {
        guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
        entityInstanceLabel: 'A label'.repeat(100),
        entityInstanceSubLabel: 'Car',
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Sublabel'.repeat(100),
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        entityInstanceLabel: 'Mark Donald',
        entityInstanceSubLabel: 'Passport',
      },
    ];
    entityDataManager.setLoadEntityInstancesResponse(
        testEntityInstancesWithLabels);

    await flushTasks();
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  async function createPage() {
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    section = document.createElement('settings-autofill-ai-section');
    section.prefs = settingsPrefs.prefs;
    document.body.appendChild(section);

    await flushTasks();
  }

  test('testLongLabelsHaveHiddenOverflow', async function() {
    await createPage();
    // Contains all labels and sublabels, in order.
    const labels =
        section.shadowRoot!.querySelectorAll<HTMLElement>('.ellipses');

    assertEquals(6, labels.length, '3 labels + 3 sublabels should be loaded');

    assertTrue(labels[0]!.textContent!.includes('A label'));
    assertGE(labels[0]!.scrollWidth, labels[0]!.offsetWidth);
    assertTrue(labels[1]!.textContent!.includes('Car'));
    assertEquals(labels[1]!.scrollWidth, labels[1]!.offsetWidth);

    assertTrue(labels[2]!.textContent!.includes('John Doe'));
    assertEquals(labels[2]!.scrollWidth, labels[2]!.offsetWidth);
    assertTrue(labels[3]!.textContent!.includes('Sublabel'));
    assertGE(labels[3]!.scrollWidth, labels[3]!.offsetWidth);

    assertTrue(labels[4]!.textContent!.includes('Mark Donald'));
    assertEquals(labels[4]!.scrollWidth, labels[4]!.offsetWidth);
    assertTrue(labels[5]!.textContent!.includes('Passport'));
    assertEquals(labels[5]!.scrollWidth, labels[5]!.offsetWidth);
  });
});
