// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {ConsentStatus, DspHotwordState, GoogleAssistantBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('GoogleAssistantHandler', function() {
  /** @type {SettingsGoogleAssistantSubpageElement} */
  let page = null;

  let browserProxy = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isAssistantAllowed: true,
      hotwordDspAvailable: true,
    });
  });

  setup(function() {
    browserProxy = TestMock.fromClass(GoogleAssistantBrowserProxyImpl);
    GoogleAssistantBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      page = document.createElement('settings-google-assistant-subpage');
      page.prefs = prefElement.prefs;
      document.body.appendChild(page);
    });
  });

  teardown(function() {
    page.remove();
  });

  test('toggleAssistant', function() {
    flush();
    const button = page.shadowRoot.querySelector('#google-assistant-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    // Tap the enable toggle button and ensure the state becomes enabled.
    button.click();
    flush();
    assertTrue(button.checked);
  });

  test('toggleAssistantContext', function() {
    let button =
        page.shadowRoot.querySelector('#google-assistant-context-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.context.enabled', false);
    flush();
    button = page.shadowRoot.querySelector('#google-assistant-context-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.context.enabled.value'));
  });

  test('toggleAssistantHotword', async function() {
    let button =
        page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    flush();
    button = page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.hotword.enabled.value'));
    await browserProxy.whenCalled('syncVoiceModelStatus');
  });

  test('hotwordToggleVisibility', function() {
    let button =
        page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    assertFalse(!!button);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    button = page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    assertTrue(!!button);
  });

  test('hotwordToggleDisabledForChildUser', function() {
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION,
      value: false,
    });

    flush();
    const button =
        page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    const indicator =
        page.shadowRoot.querySelector('#google-assistant-hotword-enable')
            .shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!button);
    assertTrue(!!indicator);
    assertTrue(button.disabled);
  });

  test('tapOnRetrainVoiceModel', async function() {
    let button = page.shadowRoot.querySelector('#retrain-voice-model');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.ACTIVITY_CONTROL_ACCEPTED);
    flush();
    button = page.shadowRoot.querySelector('#retrain-voice-model');
    assertTrue(!!button);

    button.click();
    flush();
    await browserProxy.whenCalled('retrainAssistantVoiceModel');
  });

  test('retrainButtonVisibility', function() {
    let button = page.shadowRoot.querySelector('#retrain-voice-model');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();
    button = page.shadowRoot.querySelector('#retrain-voice-model');
    assertFalse(!!button);

    // Hotword disabled.
    // Button should not be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    flush();
    button = page.shadowRoot.querySelector('#retrain-voice-model');
    assertFalse(!!button);

    // Hotword enabled.
    // Button should be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    flush();
    button = page.shadowRoot.querySelector('#retrain-voice-model');
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

    const deepLinkElement =
        page.shadowRoot.querySelector('#retrain-voice-model')
            .shadowRoot.querySelector('cr-button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Retrain model button should be focused for settingId=607.');
  });

  test('toggleAssistantNotification', function() {
    let button =
        page.shadowRoot.querySelector('#google-assistant-notification-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.notification.enabled', false);
    flush();
    button =
        page.shadowRoot.querySelector('#google-assistant-notification-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.notification.enabled.value'));
  });

  test('toggleAssistantLaunchWithMicOpen', function() {
    let button =
        page.shadowRoot.querySelector('#google-assistant-launch-with-mic-open');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.launch_with_mic_open', false);
    flush();
    button =
        page.shadowRoot.querySelector('#google-assistant-launch-with-mic-open');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.launch_with_mic_open.value'));
  });

  test('tapOnAssistantSettings', async function() {
    let button = page.shadowRoot.querySelector('#google-assistant-settings');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();
    button = page.shadowRoot.querySelector('#google-assistant-settings');
    assertTrue(!!button);

    button.click();
    flush();
    await browserProxy.whenCalled('showGoogleAssistantSettings');
  });

  test('assistantDisabledByPolicy', function() {
    let button = page.shadowRoot.querySelector('#google-assistant-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();
    button = page.shadowRoot.querySelector('#google-assistant-enable');
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

suite('GoogleAssistantHandlerWithNoDspHotword', function() {
  /** @type {SettingsGoogleAssistantSubpageElement} */
  let page = null;

  let browserProxy = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isAssistantAllowed: true,
      hotwordDspAvailable: false,
    });
  });

  setup(function() {
    browserProxy = TestMock.fromClass(GoogleAssistantBrowserProxyImpl);
    GoogleAssistantBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      page = document.createElement('settings-google-assistant-subpage');
      page.prefs = prefElement.prefs;
      document.body.appendChild(page);
      flush();
    });
  });

  teardown(function() {
    page.remove();
  });

  /**
   * @param {!HTMLElement} select
   * @param {!value} string
   */
  function selectValue(select, value) {
    select.value = value;
    select.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  test('hotwordToggleVisibilityWithNoDspHotword', function() {
    let toggle =
        page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    assertFalse(!!toggle);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    toggle = page.shadowRoot.querySelector('#google-assistant-hotword-enable');
    assertFalse(!!toggle);
  });

  test('dspHotwordDropdownVisibilityWithNoDspHotword', function() {
    let container = page.shadowRoot.querySelector('#dsp-hotword-container');
    assertFalse(!!container);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    container = page.shadowRoot.querySelector('#dsp-hotword-container');
    assertTrue(!!container);
  });

  test('dspHotwordDropdownIndicatorEnabled', function() {
    let indicator =
        page.shadowRoot.querySelector('#hotword-policy-pref-indicator');
    assertFalse(!!indicator);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
      value: true,
    });

    flush();
    const dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    indicator = page.shadowRoot.querySelector('#hotword-policy-pref-indicator');
    assertTrue(!!dropdown);
    assertFalse(!!indicator);
    assertFalse(dropdown.hasAttribute('disabled'));
  });

  test('dspHotwordDropdownIndicatorDisabled', function() {
    let indicator =
        page.shadowRoot.querySelector('#hotword-policy-pref-indicator');
    assertFalse(!!indicator);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      value: true,
    });

    flush();
    const dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    indicator = page.shadowRoot.querySelector('#hotword-policy-pref-indicator');
    assertTrue(!!dropdown);
    assertTrue(!!indicator);
    assertTrue(dropdown.hasAttribute('disabled'));
  });

  test('dspHotwordDropdownDisabledForChildUser', function() {
    let indicator =
        page.shadowRoot.querySelector('#hotword-policy-pref-indicator');
    assertFalse(!!indicator);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION,
      value: false,
    });

    flush();
    const dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    indicator = page.shadowRoot.querySelector('#hotword-policy-pref-indicator');
    assertTrue(!!dropdown);
    assertTrue(!!indicator);
    assertTrue(dropdown.disabled);
  });

  test('dspHotwordDropdownSelection', function() {
    let dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertFalse(!!dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);

    selectValue(dropdown, DspHotwordState.DEFAULT_ON);
    flush();
    assertTrue(
        page.getPref('settings.voice_interaction.hotword.enabled.value'));
    assertFalse(
        page.getPref('settings.voice_interaction.hotword.always_on.value'));

    selectValue(dropdown, DspHotwordState.ALWAYS_ON);
    flush();
    assertTrue(
        page.getPref('settings.voice_interaction.hotword.enabled.value'));
    assertTrue(
        page.getPref('settings.voice_interaction.hotword.always_on.value'));

    selectValue(dropdown, DspHotwordState.OFF);
    flush();
    assertFalse(
        page.getPref('settings.voice_interaction.hotword.enabled.value'));
    assertFalse(
        page.getPref('settings.voice_interaction.hotword.always_on.value'));
  });

  test('dspHotwordDropdownStatus', function() {
    let dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertFalse(!!dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);

    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.always_on', false);
    flush();
    assertEquals(Number(dropdown.value), DspHotwordState.DEFAULT_ON);

    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.always_on', true);
    flush();
    assertEquals(Number(dropdown.value), DspHotwordState.ALWAYS_ON);

    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    page.setPrefValue('settings.voice_interaction.hotword.always_on', false);
    flush();
    assertEquals(Number(dropdown.value), DspHotwordState.OFF);
  });

  test('dspHotwordDropdownDefaultOnSync', async function() {
    let dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertFalse(!!dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);
    selectValue(dropdown, DspHotwordState.OFF);
    flush();

    selectValue(dropdown, DspHotwordState.DEFAULT_ON);
    flush();
    await browserProxy.whenCalled('syncVoiceModelStatus');
  });

  test('dspHotwordDropdownAlwaysOnSync', async function() {
    let dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertFalse(!!dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);
    flush();

    dropdown = page.shadowRoot.querySelector('#dsp-hotword-state');
    assertTrue(!!dropdown);
    assertFalse(dropdown.disabled);
    selectValue(dropdown, DspHotwordState.OFF);
    flush();

    selectValue(dropdown, DspHotwordState.ALWAYS_ON);
    flush();
    await browserProxy.whenCalled('syncVoiceModelStatus');
  });
});
