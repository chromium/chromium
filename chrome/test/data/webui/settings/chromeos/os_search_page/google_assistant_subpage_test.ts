// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ConsentStatus, DspHotwordState, GoogleAssistantBrowserProxy, GoogleAssistantBrowserProxyImpl, SettingsGoogleAssistantSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {ControlledButtonElement, CrLinkRowElement, CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('<settings-google-assistant-subpage>', () => {
  let page: SettingsGoogleAssistantSubpageElement;
  let browserProxy: TestMock<GoogleAssistantBrowserProxy>&
      GoogleAssistantBrowserProxy;
  let prefElement: SettingsPrefsElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isAssistantAllowed: true,
      hotwordDspAvailable: true,
    });
  });

  setup(async () => {
    browserProxy = TestMock.fromClass(GoogleAssistantBrowserProxyImpl);
    GoogleAssistantBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-google-assistant-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    CrSettingsPrefs.resetForTesting();
  });

  test('toggleAssistant', () => {
    flush();
    const button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    // Tap the enable toggle button and ensure the state becomes enabled.
    button.click();
    flush();
    assertTrue(button.checked);
  });

  test('toggleAssistantContext', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-context-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.context.enabled', false);
    flush();
    button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-context-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        !!page.getPref('settings.voice_interaction.context.enabled.value'));
  });

  test('toggleAssistantHotword', async () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-hotword-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    flush();
    button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-hotword-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        !!page.getPref('settings.voice_interaction.hotword.enabled.value'));
    await browserProxy.whenCalled('syncVoiceModelStatus');
  });

  test('hotwordToggleVisibility', () => {
    let button =
        page.shadowRoot!.querySelector('#google-assistant-hotword-enable');
    assertEquals(null, button);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    button = page.shadowRoot!.querySelector('#google-assistant-hotword-enable');
    assertTrue(!!button);
  });

  test('hotwordToggleDisabledForChildUser', () => {
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION,
      value: false,
    });

    flush();
    const button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-hotword-enable');
    assertTrue(!!button);
    const indicator =
        button.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);
    assertTrue(button.disabled);
  });

  test('tapOnRetrainVoiceModel', async () => {
    let button = page.shadowRoot!.querySelector<ControlledButtonElement>(
        '#retrain-voice-model');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.ACTIVITY_CONTROL_ACCEPTED);
    flush();
    button = page.shadowRoot!.querySelector<ControlledButtonElement>(
        '#retrain-voice-model');
    assertTrue(!!button);

    button.click();
    flush();
    await browserProxy.whenCalled('retrainAssistantVoiceModel');
  });

  test('retrainButtonVisibility', () => {
    let button = page.shadowRoot!.querySelector('#retrain-voice-model');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();
    button = page.shadowRoot!.querySelector('#retrain-voice-model');
    assertEquals(null, button);

    // Hotword disabled.
    // Button should not be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    flush();
    button = page.shadowRoot!.querySelector('#retrain-voice-model');
    assertEquals(null, button);

    // Hotword enabled.
    // Button should be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    flush();
    button = page.shadowRoot!.querySelector('#retrain-voice-model');
    assertTrue(!!button);
  });

  test('Deep link to retrain voice model', async () => {
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.ACTIVITY_CONTROL_ACCEPTED);
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '607');
    Router.getInstance().navigateTo(routes.GOOGLE_ASSISTANT, params);

    const button = page.shadowRoot!.querySelector<ControlledButtonElement>(
        '#retrain-voice-model');
    assertTrue(!!button);
    const deepLinkElement = button.shadowRoot!.querySelector('cr-button');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Retrain model button should be focused for settingId=607.');
  });

  test('toggleAssistantNotification', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-notification-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.notification.enabled', false);
    flush();
    button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-notification-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(!!page.getPref(
        'settings.voice_interaction.notification.enabled.value'));
  });

  test('toggleAssistantLaunchWithMicOpen', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-launch-with-mic-open');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.launch_with_mic_open', false);
    flush();
    button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-launch-with-mic-open');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(!!page.getPref(
        'settings.voice_interaction.launch_with_mic_open.value'));
  });

  test('tapOnAssistantSettings', async () => {
    let button = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#google-assistant-settings');
    assertEquals(null, button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();
    button = page.shadowRoot!.querySelector<CrLinkRowElement>(
        '#google-assistant-settings');
    assertTrue(!!button);

    button.click();
    flush();
    await browserProxy.whenCalled('showGoogleAssistantSettings');
  });

  test('assistantDisabledByPolicy', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();
    button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#google-assistant-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertTrue(button.checked);

    page.setPrefValue('settings.assistant.disabled_by_policy', true);
    flush();
    assertTrue(!!button);
    assertTrue(button.disabled);
    assertFalse(button.checked);
  });
});

suite('<settings-google-assistant-subpage> With No Dsp Hotword', () => {
  let page: SettingsGoogleAssistantSubpageElement;
  let browserProxy: TestMock<GoogleAssistantBrowserProxy>&
      GoogleAssistantBrowserProxy;
  let prefElement: SettingsPrefsElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isAssistantAllowed: true,
      hotwordDspAvailable: false,
    });
  });

  setup(async () => {
    browserProxy = TestMock.fromClass(GoogleAssistantBrowserProxyImpl);
    GoogleAssistantBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-google-assistant-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    CrSettingsPrefs.resetForTesting();
  });

  function selectValue(select: HTMLSelectElement, value: string) {
    select.value = value;
    select.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  test('hotwordToggleVisibilityWithNoDspHotword', () => {
    let toggle =
        page.shadowRoot!.querySelector('#google-assistant-hotword-enable');
    assertEquals(null, toggle);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    toggle = page.shadowRoot!.querySelector('#google-assistant-hotword-enable');
    assertEquals(null, toggle);
  });

  test('dspHotwordDropdownVisibilityWithNoDspHotword', () => {
    let container = page.shadowRoot!.querySelector('#dsp-hotword-container');
    assertEquals(null, container);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    container = page.shadowRoot!.querySelector('#dsp-hotword-container');
    assertTrue(!!container);
  });

  test('dspHotwordDropdownIndicatorEnabled', () => {
    let indicator =
        page.shadowRoot!.querySelector('#hotword-policy-pref-indicator');
    assertEquals(null, indicator);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
      value: true,
    });

    flush();
    const dropdown = page.shadowRoot!.querySelector('#dsp-hotword-state');
    indicator =
        page.shadowRoot!.querySelector('#hotword-policy-pref-indicator');
    assertTrue(!!dropdown);
    assertEquals(null, indicator);
    assertFalse(dropdown.hasAttribute('disabled'));
  });

  test('dspHotwordDropdownIndicatorDisabled', () => {
    let indicator =
        page.shadowRoot!.querySelector('#hotword-policy-pref-indicator');
    assertEquals(null, indicator);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      value: true,
    });

    flush();
    const dropdown = page.shadowRoot!.querySelector('#dsp-hotword-state');
    indicator =
        page.shadowRoot!.querySelector('#hotword-policy-pref-indicator');
    assertTrue(!!dropdown);
    assertTrue(!!indicator);
    assertTrue(dropdown.hasAttribute('disabled'));
  });

  test('dspHotwordDropdownDisabledForChildUser', () => {
    let indicator =
        page.shadowRoot!.querySelector('#hotword-policy-pref-indicator');
    assertEquals(null, indicator);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION,
      value: false,
    });

    flush();
    const dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    indicator =
        page.shadowRoot!.querySelector('#hotword-policy-pref-indicator');
    assertTrue(!!dropdown);
    assertTrue(!!indicator);
    assertTrue(dropdown.disabled);
  });

  test('dspHotwordDropdownSelection', () => {
    let dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertEquals(null, dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);

    selectValue(dropdown, String(DspHotwordState.DEFAULT_ON));
    flush();
    assertTrue(
        !!page.getPref('settings.voice_interaction.hotword.enabled.value'));
    assertEquals(
        false,
        page.getPref('settings.voice_interaction.hotword.always_on.value'));

    selectValue(dropdown, String(DspHotwordState.ALWAYS_ON));
    flush();
    assertTrue(
        !!page.getPref('settings.voice_interaction.hotword.enabled.value'));
    assertTrue(
        !!page.getPref('settings.voice_interaction.hotword.always_on.value'));

    selectValue(dropdown, String(DspHotwordState.OFF));
    flush();
    assertEquals(
        false,
        page.getPref('settings.voice_interaction.hotword.enabled.value'));
    assertEquals(
        false,
        page.getPref('settings.voice_interaction.hotword.always_on.value'));
  });

  test('dspHotwordDropdownStatus', () => {
    let dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertEquals(null, dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);

    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.always_on', false);
    flush();
    assertEquals(DspHotwordState.DEFAULT_ON, Number(dropdown.value));

    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.always_on', true);
    flush();
    assertEquals(DspHotwordState.ALWAYS_ON, Number(dropdown.value));

    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    page.setPrefValue('settings.voice_interaction.hotword.always_on', false);
    flush();
    assertEquals(DspHotwordState.OFF, Number(dropdown.value));
  });

  test('dspHotwordDropdownDefaultOnSync', async () => {
    let dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertEquals(null, dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);
    selectValue(dropdown, String(DspHotwordState.OFF));
    flush();

    selectValue(dropdown, String(DspHotwordState.DEFAULT_ON));
    flush();
    await browserProxy.whenCalled('syncVoiceModelStatus');
  });

  test('dspHotwordDropdownAlwaysOnSync', async () => {
    let dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertEquals(null, dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown =
        page.shadowRoot!.querySelector<HTMLSelectElement>('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);
    selectValue(dropdown, String(DspHotwordState.OFF));
    flush();

    selectValue(dropdown, String(DspHotwordState.ALWAYS_ON));
    flush();
    await browserProxy.whenCalled('syncVoiceModelStatus');
  });
});
