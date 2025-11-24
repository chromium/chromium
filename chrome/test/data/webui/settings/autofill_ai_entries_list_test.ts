// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertFalse, assertGE, assertTrue, assertDeepEquals} from 'chrome://webui-test/chai_assert.js';
import {CrSettingsPrefs, ModelExecutionEnterprisePolicyValue, loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import type {CrButtonElement, SettingsAutofillAiEntriesListElement, SettingsSimpleConfirmationDialogElement, SettingsAutofillAiAddOrEditDialogElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;

suite('AutofillAiEntriesListUiReflectsEligibilityStatus', function() {
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
        type: {
          typeName: 2,
          typeNameAsString: 'Car',
          addEntityTypeString: 'Add car',
          editEntityTypeString: 'Edit car',
          deleteEntityTypeString: 'Delete car',
          supportsWalletStorage: false,
        },
        entityInstanceLabel: 'Toyota',
        entityInstanceSubLabel: 'Car',
        storedInWallet: false,
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        type: {
          typeName: 0,
          typeNameAsString: 'Passport',
          addEntityTypeString: 'Add passport',
          editEntityTypeString: 'Edit passport',
          deleteEntityTypeString: 'Delete passport',
          supportsWalletStorage: false,
        },
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Passport',
        storedInWallet: false,
      },
    ];
    entityDataManager.setLoadEntityInstancesResponse(
        testEntityInstancesWithLabels);
    // By default, the user is not opted in.
    entityDataManager.setGetOptInStatusResponse(false);
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
    {optedIn: true, ineligibleUser: true, title: 'OptedInIneligibleUser'},
    {optedIn: true, ineligibleUser: false, title: 'OptedInEligibleUser'},
    {optedIn: false, ineligibleUser: true, title: 'OptedOutIneligibleUser'},
    {optedIn: false, ineligibleUser: false, title: 'OptedOutEligibleUser'},
  ];

  async function createEntriesList(
      eligibleUser: boolean = true,
      autofillAiIgnoresWhetherAddressFillingIsEnabled: boolean =
          false): Promise<SettingsAutofillAiEntriesListElement> {
    loadTimeData.overrideValues({
      userEligibleForAutofillAi: eligibleUser,
      AutofillAiIgnoresWhetherAddressFillingIsEnabled:
          autofillAiIgnoresWhetherAddressFillingIsEnabled,
    });
    const entriesList: SettingsAutofillAiEntriesListElement =
        document.createElement('settings-autofill-ai-entries-list');
    entriesList.prefs = settingsPrefs.prefs;
    document.body.appendChild(entriesList);
    await flushTasks();
    return entriesList;
  }

  // The Opt-in status is updated when
  // `prefs.autofill.autofill_ai.opt_in_status.value changes`. However, the new
  // status's actual value is sourced from
  // `entityDataManager.getOptInStatusResponse`. To force an opt-in status
  // update, you must change the value returned by `entityDataManager` and then
  // trigger a refresh by modifying the aforementioned preference.
  function updateOptInStatus(
      newValue: boolean, entriesList: SettingsAutofillAiEntriesListElement) {
    entityDataManager.setGetOptInStatusResponse(newValue);
    entriesList.setPrefValue('autofill.autofill_ai.opt_in_status', {});
  }

  eligibilityParams.forEach(
      (params) => test(params.title, async function() {
        const entriesList = await createEntriesList(!params.ineligibleUser);
        updateOptInStatus(params.optedIn, entriesList);
        await flushTasks();

        const addButton =
            entriesList.shadowRoot!.querySelector<CrButtonElement>(
                '#addEntityInstance');
        assertTrue(!!addButton);
        assertEquals(
            addButton.disabled, params.ineligibleUser || !params.optedIn);

        assertTrue(
            isVisible(entriesList.shadowRoot!.querySelector('#entries')));
      }));

  test('DisablingClassicAutofillPrefDisablesTheFeature', async function() {
    const entriesList = await createEntriesList();
    updateOptInStatus(true, entriesList);
    await flushTasks();

    const addButton = entriesList.shadowRoot!.querySelector<CrButtonElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);

    // Check that when the autofill pref is off, the add button becomes
    // disabled, which essentially means the feature is off.
    entriesList.setPrefValue('autofill.profile_enabled', false);
    await flushTasks();
    assertTrue(addButton.disabled);
  });

  test(
      'DisablingClassicAutofillPrefDoesNotDisabledTheFeatureIfOverrideBehaviourIsEnabled',
      async function() {
        const entriesList = await createEntriesList(
            /*userEligible=*/ true,
            /*autofillAiIgnoresWhetherAddressFillingIsEnabled=*/ true);
        updateOptInStatus(true, entriesList);
        await flushTasks();

        const addButton =
            entriesList.shadowRoot!.querySelector<CrButtonElement>(
                '#addEntityInstance');
        assertTrue(!!addButton);
        assertFalse(addButton.disabled);

        entriesList.setPrefValue('autofill.profile_enabled', false);
        await flushTasks();
        assertFalse(addButton.disabled);
      });

  test('AddButtonEnabledByDefaultWhenAllowEditingPrefUnset', async function() {
    const entriesList = await createEntriesList();
    entriesList.allowEditingPref = null; // Explicitly unset
    updateOptInStatus(true, entriesList);
    await flushTasks();

    const addButton = entriesList.shadowRoot!.querySelector<CrButtonElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);
  });

  test('DisableAddButtotBasedOnAllowEditingPrefValue', async function() {
    const entriesList = await createEntriesList();
    entriesList.allowEditingPref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    };
    updateOptInStatus(true, entriesList);
    await flushTasks();

    const addButton = entriesList.shadowRoot!.querySelector<CrButtonElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);
    assertFalse(addButton.disabled);

    entriesList.set('allowEditingPref.value', false);
    await flushTasks();

    assertTrue(addButton.disabled);
  });
});

suite('AutofillAiEntriesListUiTest', function() {
  let entriesList: SettingsAutofillAiEntriesListElement;
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
    loadTimeData.overrideValues({userEligibleForAutofillAi: true});

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    testEntityInstance = {
      type: {
        typeName: 1,
        typeNameAsString: 'Driver\'s license',
        addEntityTypeString: 'Add driver\'s license',
        editEntityTypeString: 'Edit driver\'s license',
        deleteEntityTypeString: 'Delete driver\'s license',
        supportsWalletStorage: false,
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
    // Initially not sorted. The production code should sort them
    // alphabetically and put entities with Wallet storage last.
    testEntityTypes = [
      {
        typeName: 0,
        typeNameAsString: 'Passport',
        addEntityTypeString: 'Add passport',
        editEntityTypeString: 'Edit passport',
        deleteEntityTypeString: 'Delete passport',
        supportsWalletStorage: false,
      },
      {
        typeName: 6,
        typeNameAsString: 'Flight',
        addEntityTypeString: '',
        editEntityTypeString: '',
        deleteEntityTypeString: '',
        supportsWalletStorage: true,
      },
      {
        typeName: 2,
        typeNameAsString: 'Car',
        addEntityTypeString: 'Add car',
        editEntityTypeString: 'Edit car',
        deleteEntityTypeString: 'Delete car',
        supportsWalletStorage: false,
      },
    ];
    // Initially not sorted alphabetically. The production code should sort them
    // alphabetically.
    const testEntityInstancesWithLabels:
        chrome.autofillPrivate.EntityInstanceWithLabels[] = [
      {
        guid: 'e4bbe384-ee63-45a4-8df3-713a58fdc181',
        type: testEntityTypes[2]!,
        entityInstanceLabel: 'Toyota',
        entityInstanceSubLabel: 'Car',
        storedInWallet: true,
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        type: testEntityTypes[0]!,
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Passport',
        storedInWallet: false,
      },
      {
        // Note that this is the `testEntityInstance` guid.
        guid: 'd70b5bb7-49a6-4276-b4b7-b014dacdc9e6',
        type: testEntityInstance.type,
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Driver\'s license',
        storedInWallet: false,
      },
    ];
    entityDataManager.setGetOptInStatusResponse(true);
    entityDataManager.setGetWritableEntityTypesResponse(
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

  async function createEntriesList(
      allowedEntityTypes: Set<number>|null = null) {
    entriesList = document.createElement('settings-autofill-ai-entries-list');
    entriesList.prefs = settingsPrefs.prefs;
    entriesList.allowedEntityTypes = allowedEntityTypes;
    document.body.appendChild(entriesList);
    await flushTasks();

    const entityInstancesQueried =
        entriesList.shadowRoot!.querySelector<HTMLElement>('#entries');
    assertTrue(!!entityInstancesQueried);
    entityInstancesListElement = entityInstancesQueried;

    assertTrue(!!entriesList.shadowRoot!.querySelector('#entriesHeader'));
    await entityDataManager.whenCalled('loadEntityInstances');
  }

  // Tests that walletable entities have an icon button mentionining the wallet
  // type in its title. Local entities have an actionable button which allows
  // users editing and deleting.
  test('AutofillAiWalletEntitiesHaveWalletPassesIconButton', async function() {
    await createEntriesList();

    const listItems =
        entityInstancesListElement.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(4, listItems.length);

    for (let i = 0; i < listItems.length; i++) {
      const item = listItems[i]!;
      // Note that the last element is a hidden element that is only
      // visible when the user has no entities stored.
      if (i === listItems.length - 1) {
        assertFalse(isVisible(item));
        continue;
      }

      const iconButton = item.querySelector('cr-icon-button')!;
      // Only the Vehicle entity (Toyota) is stored in Wallet.
      if (!item.textContent.includes('Toyota')) {
        const labels = item.querySelectorAll<HTMLElement>('.ellipses');
        assertTrue(
            iconButton.getAttribute('title')!.includes(loadTimeData.getStringF(
                'autofillAiMoreActionsForEntityInstance', labels[0]!.innerText,
                labels[1]!.innerText)));
      } else {
        assertEquals(
            loadTimeData.getString('remoteWalletPassesLinkLabel'),
            iconButton.getAttribute('title'));
      }
    }
  });

  test('EntityInstancesLoadedAndSortedAlphabetically', async function() {
    await createEntriesList();
    const listItems =
        entityInstancesListElement.querySelectorAll<HTMLElement>('.list-item');

    assertEquals(
        4, listItems.length,
        '3 entity instances and a hidden element were loaded.');
    // The items should now also be sorted alphabetically.
    assertTrue(listItems[0]!.textContent.includes('John Doe'));
    assertTrue(listItems[0]!.textContent.includes('Driver\'s license'));
    assertTrue(listItems[1]!.textContent.includes('John Doe'));
    assertTrue(listItems[1]!.textContent.includes('Passport'));
    assertTrue(listItems[2]!.textContent.includes('Toyota'));
    assertTrue(listItems[2]!.textContent.includes('Car'));
    assertFalse(isVisible(listItems[3]!));
  });

  test('EntityInstancesFilteredWhenFilterProvided', async function() {
    await createEntriesList(new Set([
      0,  // Passport
    ]));

    const listItems =
        entityInstancesListElement.querySelectorAll<HTMLElement>('.list-item');

    // Only pasport and hidden placeholder entry should remain
    assertEquals(2, listItems.length);
    assertTrue(listItems[0]!.textContent.includes('John Doe'));
    assertTrue(listItems[0]!.textContent.includes('Passport'));
    assertFalse(isVisible(listItems[1]!));
  });

  interface RemoveEntityInstanceParams {
    // Whether the user confirms the delete dialog.
    confirmed: boolean;
    // The title of the test.
    title: string;
  }

  const removeEntityInstanceParams: RemoveEntityInstanceParams[] = [
    {confirmed: true, title: 'RemoveEntityInstanceConfirmed'},
    {confirmed: false, title: 'RemoveEntityInstanceCancelled'},
  ];

  removeEntityInstanceParams.forEach(
      (params) => test(params.title, async function() {
        await createEntriesList();
        entityDataManager.setGetEntityInstanceByGuidResponse(
            testEntityInstance);

        const actionMenuButton =
            entityInstancesListElement.querySelector<HTMLElement>(
                '#moreButton');
        assertTrue(!!actionMenuButton);
        actionMenuButton.click();
        await flushTasks();

        const deleteButton = entriesList.shadowRoot!.querySelector<HTMLElement>(
            '#menuRemoveEntityInstance');

        assertTrue(!!deleteButton);
        deleteButton.click();
        await flushTasks();

        const removeEntityInstanceDialog =
            entriesList.shadowRoot!
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

  interface AddOrEditDialogParams {
    // True if the user is adding an entity instance, false if the user is
    // editing an entity instance.
    add: boolean;
    // The title of the test.
    title: string;
  }

  const addOrEditEntityInstanceDialogParams: AddOrEditDialogParams[] = [
    {add: true, title: 'AddEntityInstanceDialogOpenAndConfirm'},
    {add: false, title: 'EditEntityInstanceDialogOpenAndConfirm'},
  ];

  addOrEditEntityInstanceDialogParams.forEach(
      (params) => test(params.title, async function() {
        await createEntriesList();
        if (params.add) {
          // Open the add entity instance dialog.
          const addButton = entriesList.shadowRoot!.querySelector<HTMLElement>(
              '#addEntityInstance');
          assertTrue(!!addButton);
          addButton.click();
          await flushTasks();

          const addSpecificEntityTypeButton =
              entriesList.shadowRoot!.querySelector<HTMLElement>(
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

          const editButton = entriesList.shadowRoot!.querySelector<HTMLElement>(
              '#menuEditEntityInstance');

          assertTrue(!!editButton);
          editButton.click();
          await flushTasks();
        }

        // Check that the dialog is populated with the correct entity instance
        // information.
        const addOrEditEntityInstanceDialog =
            entriesList.shadowRoot!
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

  test('AddButtonShowsEntityInstancesList', async function() {
    await createEntriesList();
    const addButton = entriesList.shadowRoot!.querySelector<HTMLElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);
    addButton.click();
    await flushTasks();

    const addSpecificEntityTypeButtons =
        entriesList.shadowRoot!.querySelectorAll<HTMLElement>(
            '#addSpecificEntityType');
    assertEquals(testEntityTypes.length, addSpecificEntityTypeButtons.length);
    for (const index in testEntityTypes) {
      assertTrue(
          addSpecificEntityTypeButtons[index]!.textContent.includes(
              testEntityTypes[index]!.typeNameAsString));
    }
  });

  test('AddButtonShowsSortedEntityInstancesList', async function() {
    // Exclude passports
    const allowedEntityTypes =
        testEntityTypes.filter((type) => type.typeNameAsString !== 'Passport');
    assertEquals(allowedEntityTypes.length, testEntityTypes.length - 1);

    await createEntriesList(
        new Set<number>(allowedEntityTypes.map((type) => type.typeName)));

    const addButton = entriesList.shadowRoot!.querySelector<HTMLElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);
    addButton.click();
    await flushTasks();

    const addSpecificEntityTypeButtons =
        entriesList.shadowRoot!.querySelectorAll<HTMLElement>(
            '#addSpecificEntityType');
    assertEquals(
        allowedEntityTypes.length, addSpecificEntityTypeButtons.length);
    for (let i = 0; i < allowedEntityTypes.length; i++) {
      assertTrue(
          addSpecificEntityTypeButtons[i]!.textContent.includes(
              allowedEntityTypes[i]!.typeNameAsString));
    }
  });

  test(
      'EntityInstancesChangedListenerUpdatesAndAlphabeticallySortsEntries',
      async function() {
        await createEntriesList();
        const newTestEntityInstancesWithLabels:
            chrome.autofillPrivate.EntityInstanceWithLabels[] = [
          {
            guid: 'a521fc41-d672-4947-ab39-8bc9d49b08d2',
            type: testEntityTypes.find(
                (type) => type.typeNameAsString === 'Password')!,
            entityInstanceLabel: 'Tom Clark',
            entityInstanceSubLabel: 'Passport',
            storedInWallet: false,
          },
          {
            guid: 'db56681d-9598-4e37-825c-7977f52fbcee',
            type: testEntityTypes.find(
                (type) => type.typeNameAsString === 'Car')!,
            entityInstanceLabel: 'Honda',
            entityInstanceSubLabel: 'Car',
            storedInWallet: false,
          },
          {
            guid: '1a89869f-dff2-461a-8ef8-769e0e1c66f7',
            type: testEntityInstance.type,
            entityInstanceLabel: 'Tom Clark',
            entityInstanceSubLabel: 'Driver\'s license',
            storedInWallet: false,
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
        assertTrue(listItems[0]!.textContent.includes('Honda'));
        assertTrue(listItems[0]!.textContent.includes('Car'));
        assertTrue(listItems[1]!.textContent.includes('Tom Clark'));
        assertTrue(listItems[1]!.textContent.includes('Driver\'s license'));
        assertTrue(listItems[2]!.textContent.includes('Tom Clark'));
        assertTrue(listItems[2]!.textContent.includes('Passport'));
        assertFalse(isVisible(listItems[3]!));
      });

  test(
      'EntityInstancesChangedListenerUpdatesAndFiltersEntries',
      async function() {
        // Only passports
        const allowedEntityTypes = testEntityTypes.filter(
            (type) => type.typeNameAsString === 'Passport');
        assertEquals(allowedEntityTypes.length, 1);

        await createEntriesList(
            new Set<number>(allowedEntityTypes.map((type) => type.typeName)));

        const newTestEntityInstancesWithLabels:
            chrome.autofillPrivate.EntityInstanceWithLabels[] = [
          {
            guid: 'a521fc41-d672-4947-ab39-8bc9d49b08d2',
            type: testEntityTypes.find(
                (type) => type.typeNameAsString === 'Passport')!,
            entityInstanceLabel: 'Tom Clark',
            entityInstanceSubLabel: 'Passport',
            storedInWallet: false,
          },
          {
            guid: 'db56681d-9598-4e37-825c-7977f52fbcee',
            type: testEntityTypes.find(
                (type) => type.typeNameAsString === 'Car')!,
            entityInstanceLabel: 'Honda',
            entityInstanceSubLabel: 'Car',
            storedInWallet: false,
          },
          {
            guid: '1a89869f-dff2-461a-8ef8-769e0e1c66f7',
            type: testEntityInstance.type,
            entityInstanceLabel: 'Tom Clark',
            entityInstanceSubLabel: 'Driver\'s license',
            storedInWallet: false,
          },
        ];

        entityDataManager.callEntityInstancesChangedListener(
            newTestEntityInstancesWithLabels);
        await flushTasks();

        const listItems =
            entityInstancesListElement.querySelectorAll<HTMLElement>(
                '.list-item');
        // One entity instance and a hidden element should be present.
        assertEquals(2, listItems.length);
        assertTrue(listItems[0]!.textContent.includes('Tom Clark'));
        assertTrue(listItems[0]!.textContent.includes('Passport'));
        assertFalse(isVisible(listItems[1]!));
      });

  test('EntriesDoNotDisappearAfterOptInStatusChange', async function() {
    await createEntriesList();

    const addButton = entriesList.shadowRoot!.querySelector<CrButtonElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);

    // Verify that we start in enabled state
    assertFalse(addButton.disabled);

    // Change opt-in status
    entityDataManager.setGetOptInStatusResponse(false);

    // Force opt-in status refresh
    entriesList.setPrefValue('autofill.autofill_ai.opt_in_status', {});
    await flushTasks();

    assertTrue(addButton.disabled);
    assertTrue(
        isVisible(entriesList.shadowRoot!.querySelector('#entries')),
        'With false opt-in status, the entries should be visible');
  });

  test('EntityTypesAreFilteredOnPersonalDataChangeCallback', async function() {
    await createEntriesList(new Set([
      0,  // Passport
    ]));
    const addButton = entriesList.shadowRoot!.querySelector<HTMLElement>(
        '#addEntityInstance');
    assertTrue(!!addButton);

    webUIListenerCallback('sync-status-changed');
    await flushTasks();

    addButton.click();
    await flushTasks();
    const addEntityButtons =
        entriesList.shadowRoot!.querySelectorAll<HTMLElement>(
            '#addSpecificEntityType');
    assertEquals(1, addEntityButtons.length);
  });
});

suite('AutofillAiEntriesListLongLabelsUiTest', function() {
  let entriesList: SettingsAutofillAiEntriesListElement;
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
        type: {
          typeName: 2,
          typeNameAsString: 'Car',
          addEntityTypeString: 'Add car',
          editEntityTypeString: 'Edit car',
          deleteEntityTypeString: 'Delete car',
          supportsWalletStorage: false,
        },
        entityInstanceLabel: 'A label'.repeat(100),
        entityInstanceSubLabel: 'Car',
        storedInWallet: false,
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        type: {
          typeName: 0,
          typeNameAsString: 'Passport',
          addEntityTypeString: 'Add passport',
          editEntityTypeString: 'Edit passport',
          deleteEntityTypeString: 'Delete passport',
          supportsWalletStorage: false,
        },
        entityInstanceLabel: 'John Doe',
        entityInstanceSubLabel: 'Sublabel'.repeat(100),
        storedInWallet: false,
      },
      {
        guid: '1fd09cdc-35b8-4367-8f1a-18c8c0733af0',
        type: {
          typeName: 0,
          typeNameAsString: 'Passport',
          addEntityTypeString: 'Add passport',
          editEntityTypeString: 'Edit passport',
          deleteEntityTypeString: 'Delete passport',
          supportsWalletStorage: false,
        },
        entityInstanceLabel: 'Mark Donald',
        entityInstanceSubLabel: 'Passport',
        storedInWallet: false,
      },
    ];
    entityDataManager.setLoadEntityInstancesResponse(
        testEntityInstancesWithLabels);

    await flushTasks();
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  async function createEntriesList() {
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    entriesList = document.createElement('settings-autofill-ai-entries-list');
    entriesList.prefs = settingsPrefs.prefs;
    document.body.appendChild(entriesList);

    await flushTasks();
  }

  test('LongLabelsHaveHiddenOverflow', async function() {
    await createEntriesList();
    // Contains all labels and sublabels, in order.
    const labels =
        entriesList.shadowRoot!.querySelectorAll<HTMLElement>('.ellipses');

    assertEquals(6, labels.length, '3 labels + 3 sublabels should be loaded');

    assertTrue(labels[0]!.textContent.includes('A label'));
    assertGE(labels[0]!.scrollWidth, labels[0]!.offsetWidth);
    assertTrue(labels[1]!.textContent.includes('Car'));
    assertEquals(labels[1]!.scrollWidth, labels[1]!.offsetWidth);

    assertTrue(labels[2]!.textContent.includes('John Doe'));
    assertEquals(labels[2]!.scrollWidth, labels[2]!.offsetWidth);
    assertTrue(labels[3]!.textContent.includes('Sublabel'));
    assertGE(labels[3]!.scrollWidth, labels[3]!.offsetWidth);

    assertTrue(labels[4]!.textContent.includes('Mark Donald'));
    assertEquals(labels[4]!.scrollWidth, labels[4]!.offsetWidth);
    assertTrue(labels[5]!.textContent.includes('Passport'));
    assertEquals(labels[5]!.scrollWidth, labels[5]!.offsetWidth);
  });
});
