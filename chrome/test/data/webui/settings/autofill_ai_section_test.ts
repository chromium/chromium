// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import { flushTasks } from 'chrome://webui-test/polymer_test_util.js';
import { assertEquals, assertFalse, assertTrue } from 'chrome://webui-test/chai_assert.js';
// <if expr="is_win or is_macosx or is_chromeos">
import { isVisible } from 'chrome://webui-test/test_util.js';
// </if>
import { CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue } from 'chrome://settings/settings.js';
import type { SettingsAiLoggingInfoBullet, SettingsPrefsElement, SettingsToggleButtonElement } from 'chrome://settings/settings.js';
import type { SettingsAutofillAiSectionElement } from 'chrome://settings/lazy_load.js';
import { AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl } from 'chrome://settings/lazy_load.js';

import { TestEntityDataManagerProxy } from './test_entity_data_manager_proxy.js';
// clang-format on

const AttributeTypeDataType = chrome.autofillPrivate.AttributeTypeDataType;

suite('AutofillAiSectionUiReflectsEligibilityStatus', function() {
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

  async function createSection(
      eligibleUser: boolean = true,
      autofillAddOtherDatatypesPrefIsEnabled: boolean =
          false): Promise<SettingsAutofillAiSectionElement> {
    loadTimeData.overrideValues({
      userEligibleForAutofillAi: eligibleUser,
      AutofillAddOtherDatatypesPrefIsEnabled:
          autofillAddOtherDatatypesPrefIsEnabled,
    });
    const section: SettingsAutofillAiSectionElement =
        document.createElement('settings-autofill-ai-section');
    settingsPrefs.set(
        `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    section.prefs = settingsPrefs.prefs;
    document.body.appendChild(section);

    await flushTasks();
    return section;
  }

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

  eligibilityParams.forEach(
      (params) => test(params.title, async function() {
        entityDataManager.setGetOptInStatusResponse(params.optedIn);
        const section = await createSection(!params.ineligibleUser);

        const toggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#prefToggle');
        assertTrue(!!toggle);
        assertEquals(toggle.disabled, params.ineligibleUser);
        assertEquals(toggle.checked, !params.ineligibleUser && params.optedIn);
      }));

  test('SwitchingToggleUpdatesPref', async function() {
    const section = await createSection();
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

  test('DisablingClassicAutofillPrefDisablesTheFeature', async function() {
    entityDataManager.setGetOptInStatusResponse(true);
    const section = await createSection();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#prefToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    section.set('prefs.autofill.profile_enabled.value', false);
    await flushTasks();

    // Check that when the autofill pref is off, the feature is disabled.
    assertTrue(!!toggle);
    assertFalse(toggle.checked);
  });

  test(
      'DisablingClassicAutofillPrefDoesNotDisablTheFeatureIfOtherDatatypesPrefIsEnabled',
      async function() {
        entityDataManager.setGetOptInStatusResponse(true);
        const section = await createSection(
            /*eligibleUser=*/ true,
            /*autofillAddOtherDatatypesPrefIsEnabled=*/ true);

        const toggle =
            section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#prefToggle');
        assertTrue(!!toggle);
        assertTrue(toggle.checked);

        section.set('prefs.autofill.profile_enabled.value', false);
        await flushTasks();

        // Check that even when the address autofill pref is off, the feature is
        // enabled.
        assertTrue(!!toggle);
        assertTrue(toggle.checked);
      });
});

suite('AutofillAiSectionUiTest', function() {
  let section: SettingsAutofillAiSectionElement;
  let entityDataManager: TestEntityDataManagerProxy;
  let testEntityInstance: chrome.autofillPrivate.EntityInstance;
  let testEntityTypes: chrome.autofillPrivate.EntityType[];
  let settingsPrefs: SettingsPrefsElement;
  // Note that authentication is not available on linux.
  // <if expr="is_win or is_macosx or is_chromeos">
  const authenticationPref =
      'prefs.autofill.autofill_ai.reauth_before_viewing_sensitive_data';
  // </if>

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
      shouldAuthenticateToView: false,
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
        supportsWalletStorage: false,
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
        type: testEntityTypes[1]!,
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

  async function createSection(autofillAiAvailableByDefault: boolean = false) {
    loadTimeData.overrideValues({
      userEligibleForAutofillAi: true,
      autofillAiAvailableByDefault: autofillAiAvailableByDefault,
    });
    section = document.createElement('settings-autofill-ai-section');
    section.prefs = settingsPrefs.prefs;
    document.body.appendChild(section);
    await flushTasks();
  }

  test('AutofillAiAvailableByDefaultFalseRendersExpectedUI', async function() {
    await createSection(/*autofillAiAvailableByDefault=*/ false);

    const firstColumn = section.shadowRoot!.querySelector('.column');
    assertTrue(!!firstColumn);
    const bulletsInFirstColumn = firstColumn.querySelectorAll('li');
    assertEquals(2, bulletsInFirstColumn.length);

    const firstBullet = bulletsInFirstColumn.item(0);
    assertTrue(firstBullet !== null);
    const firstBulletIcon = firstBullet.querySelector('cr-icon');
    assertTrue(!!firstBulletIcon);
    assertEquals('settings20:sync-saved-locally', firstBulletIcon.icon);
    const firstBulletText =
        firstBullet.querySelector('.cr-secondary-text')!.textContent.trim();
    assertEquals(
        loadTimeData.getString('autofillAiWhenOnUseToFill'), firstBulletText);

    const secondBullet = bulletsInFirstColumn.item(1);
    assertTrue(secondBullet !== null);
    const secondBulletIcon = secondBullet.querySelector('cr-icon');
    assertTrue(!!secondBulletIcon);
    assertEquals('settings20:text-analysis', secondBulletIcon.icon);
    const secondBulletText =
        secondBullet.querySelector('.cr-secondary-text')!.textContent.trim();
    assertEquals(
        loadTimeData.getString('autofillAiWhenOnUseToFill'), secondBulletText);
  });

  test('AutofillAiAvailableByDefaultTrue', async function() {
    await createSection(/*autofillAiAvailableByDefault=*/ true);

    const firstColumn = section.shadowRoot!.querySelector('.column');
    assertTrue(!!firstColumn);
    const bulletsInFirstColumn = firstColumn.querySelectorAll('li');
    assertEquals(1, bulletsInFirstColumn.length);

    const firstBullet = bulletsInFirstColumn.item(0);
    assertTrue(firstBullet !== null);
    const firstBulletIcon = firstBullet.querySelector('cr-icon');
    assertTrue(!!firstBulletIcon);
    assertEquals('settings20:text-analysis', firstBulletIcon.icon);
    const firstBulletText =
        firstBullet.querySelector('.cr-secondary-text')!.textContent.trim();
    assertEquals(
        loadTimeData.getString('autofillAiWhenOnCanFillDifficultFields'),
        firstBulletText);
  });

  test(
      'AutofillAiEnterpriseUserLoggingAllowedAndNonEnterpriseUserHaveNoLoggingInfoBullet',
      async function() {
        // Both enterprise and non enterprise users have the pref set to 0
        // (allow).
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.ALLOW);
        await createSection();

        const enterpriseLogginInfoBullet =
            section.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertFalse(!!enterpriseLogginInfoBullet);
      });

  test(
      'AutofillAiEnterpriseUserLoggingNotAllowedHaveLoggingInfoBullet',
      async function() {
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
        await createSection();

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
      'AutofillAiEnterpriseUserDisabledHasLoggingInfoBullet', async function() {
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.DISABLE);
        await createSection();

        const enterpriseLogginInfoBullet =
            section.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertTrue(!!enterpriseLogginInfoBullet);
        assertEquals(
            loadTimeData.getString(
                'autofillAiSubpageSublabelLoggingManagedDisabled'),
            enterpriseLogginInfoBullet.loggingManagedDisabledCustomLabel);
      });

  test('ToggleIsDisabledWhenUserIsNotEligible', async function() {
    await createSection();
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

  test('WalletablePassDetectionToggleVisibleWhenEligible', async function() {
    loadTimeData.overrideValues(
        {isUserEligibleForWalletablePassDetection: true});
    await createSection();
    const component = section.shadowRoot!.querySelector<HTMLElement>(
        '#walletablePassDetectionToggle');
    assertTrue(!!component);
  });

  test('WalletablePassDetectionToggleHiddenWhenNotEligible', async function() {
    loadTimeData.overrideValues(
        {isUserEligibleForWalletablePassDetection: false});
    await createSection();
    const component = section.shadowRoot!.querySelector<HTMLElement>(
        '#walletablePassDetectionToggle');
    assertFalse(!!component);
  });

  // <if expr="is_win or is_macosx or is_chromeos">
  test('AutofillAiReauthToggleHiddenWhenFeatureDisabled', async function() {
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: false});
    await createSection();
    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#optInAuthenticationToggle');
    assertFalse(isVisible(toggle));
  });

  test('AutofillAiReauthToggleVisibleWhenFeatureEnabled', async function() {
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: true});
    await createSection();
    await flushTasks();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#optInAuthenticationToggle');
    assertTrue(isVisible(toggle));
  });

  test('AutofillAiReauthToggleCallsUpdatePrefMethod', async function() {
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: true});
    await createSection();
    await flushTasks();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#optInAuthenticationToggle');
    assertTrue(!!toggle);

    section.set(authenticationPref, {
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    });
    await flushTasks();
    assertFalse(toggle.checked);

    toggle.click();
    await flushTasks();
    await entityDataManager.whenCalled('toggleAutofillAiReauthRequirement');
  });

  test('AutofillAiReauthToggleDisabledWhenUserIneligible', async function() {
    loadTimeData.overrideValues({
      autofillAiReauthOnViewingSensitiveDataEnabled: true,
      userEligibleForAutofillAi: false,
    });
    await createSection();
    section.set('ineligibleUser', true);
    await flushTasks();

    const toggle =
        section.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#optInAuthenticationToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);

    toggle.click();
    await flushTasks();
    assertEquals(
        0, entityDataManager.getCallCount('toggleAutofillAiReauthRequirement'));
  });
  // </if>
});
