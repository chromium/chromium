// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrCollapseElement, CrExpandButtonElement} from 'chrome://settings/lazy_load.js';
import {AiEnterpriseFeaturePrefName, EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {CollapsibleCardElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue} from 'chrome://settings/settings.js';
import type {SettingsAiLoggingInfoBullet, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';

function setupDefaultPrefs(settingsPrefs: SettingsPrefsElement) {
  settingsPrefs.set(
      `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
      ModelExecutionEnterprisePolicyValue.ALLOW);
  settingsPrefs.set(
      'prefs.optimization_guide.model_execution.autofill_prediction_improvements_enterprise_policy_allowed.value',
      ModelExecutionEnterprisePolicyValue.ALLOW);
  settingsPrefs.set('prefs.autofill.profile_enabled.value', true);
}

suite('CollapsibleAutofillSettingsCard', function() {
  let entityDataManager: TestEntityDataManagerProxy;
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
    entityDataManager.setGetOptInStatusResponse(false);

    setupDefaultPrefs(settingsPrefs);
    loadTimeData.overrideValues({userEligibleForAutofillAi: false});
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  async function createCollapsibleAutofillSettingsCard(
      eligibleUser: boolean = true,
      autofillAddOtherDatatypesPrefIsEnabled: boolean = false,
      optInStatusResponse: boolean = true,
      autofillAiAvailableByDefault: boolean =
          false): Promise<CollapsibleCardElement> {
    entityDataManager.setGetOptInStatusResponse(optInStatusResponse);
    loadTimeData.overrideValues({
      userEligibleForAutofillAi: eligibleUser,
      AutofillAddOtherDatatypesPrefIsEnabled:
          autofillAddOtherDatatypesPrefIsEnabled,
      autofillAiAvailableByDefault: autofillAiAvailableByDefault,
    });

    const card: CollapsibleCardElement =
        document.createElement('collapsible-autofill-settings-card');
    card.prefs = settingsPrefs.prefs;
    document.body.appendChild(card);

    await flushTasks();
    return card;
  }

  interface EligibilityParamsInterface {
    // Whether the user is opted into Autofill with Ai.
    enhancedAutofillOptedIn: boolean;
    // Whether the user is eligible for Autofill with Ai.
    enhancedAutofillEligibleUser: boolean;
    // The title of the test.
    title: string;
  }

  const enhancedAutofillEligibilityParams: EligibilityParamsInterface[] = [
    {
      enhancedAutofillOptedIn: true,
      enhancedAutofillEligibleUser: true,
      title: 'OptedInEligibleUser',
    },
    {
      enhancedAutofillOptedIn: true,
      enhancedAutofillEligibleUser: false,
      title: 'OptedInIneligibleUser',
    },
    {
      enhancedAutofillOptedIn: false,
      enhancedAutofillEligibleUser: true,
      title: 'OptedOutEligibleUser',
    },
    {
      enhancedAutofillOptedIn: false,
      enhancedAutofillEligibleUser: false,
      title: 'OptedOutIneligibleUser',
    },
  ];

  enhancedAutofillEligibilityParams.forEach((params) => {
    test(params.title, async function() {
      const card = await createCollapsibleAutofillSettingsCard(
          params.enhancedAutofillEligibleUser,
          /*autofillAddOtherDatatypesPrefIsEnabled=*/ false,
          params.enhancedAutofillOptedIn);

      const toggle =
          card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#optInToggle');
      assertTrue(!!toggle);
      assertEquals(!params.enhancedAutofillEligibleUser, toggle.disabled);
      assertEquals(
          params.enhancedAutofillEligibleUser && params.enhancedAutofillOptedIn,
          toggle.checked);
    });
  });

  test('AutofillAiAvailableByDefaultFalseRendersExpectedUI', async function() {
    const card = await createCollapsibleAutofillSettingsCard(
        /*eligibleUser=*/ true,
        /*autofillAddOtherDatatypesPrefIsEnabled=*/ false,
        /*optInStatusResponse=*/ true,
        /*autofillAiAvailableByDefault=*/ false);

    const firstColumn = card.shadowRoot!.querySelector('.column');
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

  test('AutofillAiAvailableByDefaultTrueRendersExpectedUI', async function() {
    const card = await createCollapsibleAutofillSettingsCard(
        /*eligibleUser=*/ true,
        /*autofillAddOtherDatatypesPrefIsEnabled=*/ false,
        /*optInStatusResponse=*/ true,
        /*autofillAiAvailableByDefault=*/ true);

    const firstColumn = card.shadowRoot!.querySelector('.column');
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

  test('RendersHeader', async function() {
    const card = await createCollapsibleAutofillSettingsCard();
    const headerText = card.shadowRoot!.querySelector('#header-text');
    assertTrue(!!headerText);
    const mainLabel = headerText.querySelector('div:not(.cr-secondary-text)');
    assertTrue(!!mainLabel);
    assertEquals(
        loadTimeData.getString('yourSavedInfoAutofillSettingsLabel'),
        mainLabel.textContent.trim());
    const subLabel = headerText.querySelector('.cr-secondary-text');
    assertTrue(!!subLabel);
    assertEquals(
        loadTimeData.getString('yourSavedInfoAutofillSettingsDescription'),
        subLabel.textContent.trim());
  });


  test('SwitchingToggleUpdatesPref', async function() {
    // The user is eligible so that the toggle is enabled, however they are not
    // opted in.
    const card = await createCollapsibleAutofillSettingsCard(
        /*eligibleUser=*/ true,
        /*autofillAddOtherDatatypesPrefIsEnabled=*/ false,
        /*optInStatusResponse=*/ false);

    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#optInToggle');
    assertTrue(!!toggle);

    toggle.click();
    await flushTasks();
    assertTrue(await entityDataManager.whenCalled('setOptInStatus'));
    entityDataManager.reset();
    await flushTasks();

    toggle.click();
    assertFalse(await entityDataManager.whenCalled('setOptInStatus'));
  });

  test('IsCollapsedByDefaultAndContentIsHidden', async function() {
    const card = await createCollapsibleAutofillSettingsCard();
    const expandButton = card.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    assertFalse(expandButton.expanded, 'Expand button should be collapsed');

    const collapseSection = card.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!collapseSection);
    assertFalse(collapseSection.opened, 'Collapse section should be closed');
    assertFalse(
        isVisible(collapseSection), 'Collapse section should be hidden');
  });

  test('ExpandsAndCollapsesWhenHeaderIsClicked', async function() {
    const card = await createCollapsibleAutofillSettingsCard();

    const expandButton = card.shadowRoot!.querySelector<CrExpandButtonElement>(
        'cr-expand-button');
    assertTrue(!!expandButton);
    const collapseSection =
        card.shadowRoot!.querySelector<CrCollapseElement>('#expandedContent');
    assertTrue(!!collapseSection);

    assertFalse(expandButton.expanded);
    assertFalse(collapseSection.opened);

    expandButton.click();
    await flush();

    assertTrue(expandButton.expanded);
    assertTrue(collapseSection.opened);

    expandButton.click();
    await flush();

    assertFalse(expandButton.expanded);
    assertFalse(collapseSection.opened);

    expandButton.click();
    await flush();

    assertTrue(expandButton.expanded);
    assertTrue(collapseSection.opened);
  });

  test(
      'AutofillAiEnterpriseUserLoggingAllowedAndNonEnterpriseUserHaveNoLoggingInfoBullet',
      async function() {
        // Both enterprise and non enterprise users have the pref set to 0
        // (allow).
        const card = await createCollapsibleAutofillSettingsCard();

        const enterpriseLogginInfoBullet =
            card.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertFalse(!!enterpriseLogginInfoBullet);
      });

  test(
      'AutofillAiEnterpriseUserLoggingNotAllowedHaveLoggingInfoBullet',
      async function() {
        settingsPrefs.set(
            `prefs.${AiEnterpriseFeaturePrefName.AUTOFILL_AI}.value`,
            ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
        const card = await createCollapsibleAutofillSettingsCard();

        const enterpriseLogginInfoBullet =
            card.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
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
        const card = await createCollapsibleAutofillSettingsCard();

        const enterpriseLogginInfoBullet =
            card.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
                '#enterpriseInfoBullet');
        assertTrue(!!enterpriseLogginInfoBullet);
        assertEquals(
            loadTimeData.getString(
                'autofillAiSubpageSublabelLoggingManagedDisabled'),
            enterpriseLogginInfoBullet.loggingManagedDisabledCustomLabel);
      });

  test('ToggleIsDisabledWhenUserIsNotEligible', async function() {
    const card = await createCollapsibleAutofillSettingsCard();
    // The toggle is initially enabled (see the setup() method). Clicking it
    // sets the opt-in status to false.
    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#optInToggle');
    assertTrue(!!toggle);
    assertFalse(toggle.disabled);
    assertTrue(toggle.checked);

    // Simulate a toggle click that fails because the user meanwhile became
    // ineligible for Autofill AI.
    entityDataManager.setSetOptInStatusResponse(false);
    assertTrue(toggle.checked, 'Toggle should be checked before click');
    toggle.click();
    await flushTasks();

    assertFalse(
        toggle.checked,
        'Toggle should be unchecked after the click event has propagated.');
    const optInStatus = await entityDataManager.whenCalled('setOptInStatus');
    assertFalse(optInStatus);

    await flushTasks();
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);
  });

  test('DisablingClassicAutofillPrefDisablesTheFeature', async function() {
    const card = await createCollapsibleAutofillSettingsCard();

    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#optInToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    card.set('prefs.autofill.profile_enabled.value', false);
    await flushTasks();

    // Check that when the autofill pref is off, the feature is disabled.
    assertTrue(!!toggle);
    assertFalse(toggle.checked);
  });

  test(
      'DisablingClassicAutofillPrefDoesNotDisabledTheFeatureIfOverrideBehaviourIsEnabled',
      async function() {
        const card = await createCollapsibleAutofillSettingsCard(
            /*eligibleUser=*/ true,
            /*autofillAddOtherDatatypesPrefIsEnabled=*/ true);

        const toggle =
            card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#optInToggle');
        assertTrue(!!toggle);
        assertTrue(toggle.checked);

        card.set('prefs.autofill.profile_enabled.value', false);
        await flushTasks();

        // Check that even when the address autofill pref is off, the feature is
        // enabled.
        assertTrue(!!toggle);
        assertTrue(toggle.checked);
      });

  test('EnterprisePolicyObserver', async function() {
    const card = await createCollapsibleAutofillSettingsCard();

    // Expand the card to make the logging bullet visible.
    const expandButton = card.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    expandButton.click();
    await flushTasks();

    const getLoggingBullet = () =>
        card.shadowRoot!.querySelector<SettingsAiLoggingInfoBullet>(
            '#enterpriseInfoBullet');

    // Initial state: Policy `ALLOW`.
    assertTrue(card.get('enhancedAutofillOptedIn_.value'));
    assertEquals(undefined, card.get('enhancedAutofillOptedIn_.enforcement'));
    assertEquals(undefined, card.get('enhancedAutofillOptedIn_.controlledBy'));
    assertFalse(!!getLoggingBullet());

    // State: Policy `DISABLE`.
    card.setPrefValue(
        AiEnterpriseFeaturePrefName.AUTOFILL_AI,
        ModelExecutionEnterprisePolicyValue.DISABLE);
    await flushTasks();

    assertFalse(card.get('enhancedAutofillOptedIn_.value'));
    assertEquals(
        chrome.settingsPrivate.Enforcement.ENFORCED,
        card.get('enhancedAutofillOptedIn_.enforcement'));
    assertEquals(
        chrome.settingsPrivate.ControlledBy.USER_POLICY,
        card.get('enhancedAutofillOptedIn_.controlledBy'));
    assertTrue(!!getLoggingBullet());

    // State: Policy `ALLOW` again.
    card.setPrefValue(
        AiEnterpriseFeaturePrefName.AUTOFILL_AI,
        ModelExecutionEnterprisePolicyValue.ALLOW);
    await flushTasks();

    assertTrue(card.get('enhancedAutofillOptedIn_.value'));
    assertEquals(undefined, card.get('enhancedAutofillOptedIn_.enforcement'));
    assertEquals(undefined, card.get('enhancedAutofillOptedIn_.controlledBy'));
    assertFalse(!!getLoggingBullet());
  });

  test('WalletablePassDetectionToggleVisibleWhenEligible', async function() {
    loadTimeData.overrideValues(
        {isUserEligibleForWalletablePassDetection: true});
    const card = await createCollapsibleAutofillSettingsCard();
    const component = card.shadowRoot!.querySelector<HTMLElement>(
        '#walletablePassDetectionToggle');
    assertTrue(!!component);
  });

  test('WalletablePassDetectionToggleHiddenWhenNotEligible', async function() {
    loadTimeData.overrideValues(
        {isUserEligibleForWalletablePassDetection: false});
    const card = await createCollapsibleAutofillSettingsCard();
    const component = card.shadowRoot!.querySelector<HTMLElement>(
        '#walletablePassDetectionToggle');
    assertFalse(!!component);
  });

  // <if expr="is_win or is_macosx or is_chromeos">
  test('AutofillAiReauthToggleHiddenWhenFeatureDisabled', async function() {
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: false});
    const card = await createCollapsibleAutofillSettingsCard();
    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#optInAuthenticationToggle');
    assertFalse(isVisible(toggle));
  });

  test('AutofillAiReauthToggleVisibleWhenFeatureEnabled', async function() {
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: true});
    const card = await createCollapsibleAutofillSettingsCard();
    await flushTasks();

    const expandButton = card.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    expandButton.click();
    await flushTasks();

    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#optInAuthenticationToggle');
    assertTrue(isVisible(toggle));
  });

  test('AutofillAiReauthToggleCallsUpdatePrefMethod', async function() {
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: true});
    const card = await createCollapsibleAutofillSettingsCard();
    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#optInAuthenticationToggle');
    assertTrue(!!toggle);

    card.set(authenticationPref, {
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
    loadTimeData.overrideValues(
        {autofillAiReauthOnViewingSensitiveDataEnabled: true});
    const card = await createCollapsibleAutofillSettingsCard(
        /*eligibleUser=*/ false);
    const toggle = card.shadowRoot!.querySelector<SettingsToggleButtonElement>(
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
