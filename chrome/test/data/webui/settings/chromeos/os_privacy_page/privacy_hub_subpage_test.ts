// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MediaDevicesProxy, PrivacyHubBrowserProxyImpl, SettingsPrivacyHubSubpage} from 'chrome://os-settings/lazy_load.js';
import {MetricsConsentBrowserProxyImpl, OsSettingsPrivacyPageElement, Router, routes, SecureDnsMode, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMediaDevices} from '../fake_media_devices.js';
import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {DEVICE_METRICS_CONSENT_PREF_NAME, TestMetricsConsentBrowserProxy} from './test_metrics_consent_browser_proxy.js';
import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

const USER_METRICS_CONSENT_PREF_NAME = 'metrics.user_consent';

const PrivacyHubVersion = {
  Future: 'Privacy Hub with future (after MVP) features.',
  MVP: 'Privacy Hub with MVP features.',
  Dogfood: 'Privacy Hub with dogfooded features (camera and microphone only).',
};

function overridedValues(privacyHubVersion: string) {
  switch (privacyHubVersion) {
    case PrivacyHubVersion.Future: {
      return {
        showPrivacyHubPage: true,
        showPrivacyHubMVPPage: true,
        showPrivacyHubFuturePage: true,
        showSpeakOnMuteDetectionPage: true,
      };
    }
    case PrivacyHubVersion.Dogfood: {
      return {
        showPrivacyHubPage: true,
        showPrivacyHubMVPPage: false,
        showPrivacyHubFuturePage: false,
        showSpeakOnMuteDetectionPage: true,
      };
    }
    case PrivacyHubVersion.MVP: {
      return {
        showPrivacyHubPage: true,
        showPrivacyHubMVPPage: true,
        showPrivacyHubFuturePage: false,
        showSpeakOnMuteDetectionPage: true,
      };
    }
    default: {
      assertNotReached(`Unsupported Privacy Hub version: {privacyHubVersion}`);
    }
  }
}

async function parametrizedPrivacyHubSubpageTestsuite(
    privacyHubVersion: string) {
  let privacyHubSubpage: SettingsPrivacyHubSubpage;
  let privacyHubBrowserProxy: TestPrivacyHubBrowserProxy;
  let mediaDevices: FakeMediaDevices;

  setup(async () => {
    loadTimeData.overrideValues(overridedValues(privacyHubVersion));

    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    mediaDevices = new FakeMediaDevices();
    MediaDevicesProxy.setMediaDevicesForTesting(mediaDevices);

    privacyHubSubpage = document.createElement('settings-privacy-hub-subpage');
    document.body.appendChild(privacyHubSubpage);
    await waitAfterNextRender(privacyHubSubpage);
    flush();
  });

  teardown(() => {
    privacyHubSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to camera toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1116');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot!.querySelector('#cameraToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Camera toggle should be focused for settingId=1116.');
  });

  test('Deep link to microphone toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1117');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot!.querySelector('#microphoneToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Microphone toggle should be focused for settingId=1117.');
  });

  test('Microphone toggle disabled when the hw toggle is active', async () => {
    const getMicrophoneList = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#micList');
    const getMicrophoneCrToggle = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#microphoneToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    const getMicrophoneTooltip = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#microphoneToggle')!
            .querySelector('cr-tooltip-icon');

    privacyHubBrowserProxy.microphoneToggleIsEnabled = false;
    await privacyHubBrowserProxy.whenCalled(
        'getInitialMicrophoneHardwareToggleState');

    await waitAfterNextRender(privacyHubSubpage);
    // There should be no MediaDevice connected initially. Microphone toggle
    // should be disabled as no microphone is connected.
    assertEquals(null, getMicrophoneList());
    assertTrue(getMicrophoneCrToggle()!.disabled);
    // TODO(b/259553116) Check how banshee handles the microphone hardware
    // switch.
    assertTrue(getMicrophoneTooltip()!.hidden);

    // Add a microphone.
    mediaDevices.addDevice('audioinput', 'Fake Microphone');
    await waitAfterNextRender(privacyHubSubpage);
    assert(getMicrophoneList());
    // Microphone toggle should be enabled to click now as there is a microphone
    // connected and the hw toggle is inactive.
    assertFalse(getMicrophoneCrToggle()!.disabled);
    // The tooltip should only show when the HW switch is engaged.
    assertTrue(getMicrophoneTooltip()!.hidden);

    // Activate the hw toggle.
    webUIListenerCallback('microphone-hardware-toggle-changed', true);
    await waitAfterNextRender(privacyHubSubpage);
    // Microphone toggle should be disabled again due to the hw switch being
    // active.
    assertTrue(getMicrophoneCrToggle()!.disabled);
    // With the HW switch being active the tooltip should be visible.
    assertFalse(getMicrophoneTooltip()!.hidden);
    // Ensure that the tooltip has the intended content.
    assertEquals(
        privacyHubSubpage.i18n('microphoneHwToggleTooltip'),
        getMicrophoneTooltip()!.tooltipText.trim());

    mediaDevices.popDevice();
  });

  test('Suggested content, pref disabled', () => {
    privacyHubSubpage.remove();
    privacyHubSubpage = document.createElement('settings-privacy-hub-subpage');
    document.body.appendChild(privacyHubSubpage);
    flush();

    // The default state of the pref is disabled.
    const suggestedContent =
        privacyHubSubpage.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#suggested-content');
    assert(suggestedContent);
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', () => {
    // Update the backing pref to enabled.
    privacyHubSubpage.prefs = {
      'settings': {
        'suggested_content_enabled': {
          value: true,
        },
      },
    };

    flush();

    // The checkbox reflects the updated pref state.
    const suggestedContent =
        privacyHubSubpage.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#suggested-content');
    assert(suggestedContent);
    assertTrue(suggestedContent.checked);
  });

  test('Deep link to Geolocation toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1118');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const toggleElement =
        privacyHubSubpage.shadowRoot!.querySelector('#geolocationToggle');
    if (privacyHubVersion === PrivacyHubVersion.Dogfood) {
      assertEquals(null, toggleElement);
    } else {
      assert(toggleElement);
      const deepLinkElement =
          toggleElement.shadowRoot!.querySelector('cr-toggle');
      assert(deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Geolocation toggle should be focused for settingId=1118.');
    }
  });

  test('Deep link to speak-on-mute toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    const settingId = settingMojom.Setting.kSpeakOnMuteDetectionOnOff;
    params.append('settingId', `${settingId}`);
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement = privacyHubSubpage.shadowRoot!
                                .querySelector('#speakonmuteDetectionToggle')!
                                .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `Speak-on-mute detection toggle should be focused for settingId=${
            settingId}.`);
  });

  test('Camera, microphone toggle, their sublabel and their list', async () => {
    const getNoCameraText = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#noCamera');
    const getCameraList = () =>
        privacyHubSubpage.shadowRoot!.querySelector<DomRepeat>('#cameraList');
    const getCameraCrToggle = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#cameraToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    const getCameraToggleSublabel = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#cameraToggle')!
            .shadowRoot!.querySelector('#sub-label-text');


    const getNoMicrophoneText = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#noMic');
    const getMicrophoneList = () =>
        privacyHubSubpage.shadowRoot!.querySelector<DomRepeat>('#micList');
    const getMicrophoneCrToggle = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#microphoneToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    const getMicrophoneToggleSublabel = () =>
        privacyHubSubpage.shadowRoot!.querySelector('#microphoneToggle')!
            .shadowRoot!.querySelector('#sub-label-text');

    // Initially, the lists of media devices should be hidden and `#noMic` and
    // `#noCamera` should be displayed.
    assertEquals(null, getCameraList());
    assert(getNoCameraText());
    assertEquals(
        privacyHubSubpage.i18n('noCameraConnectedText'),
        getNoCameraText()!.textContent!.trim());
    assertEquals(
        privacyHubSubpage.i18n('cameraToggleSubtext'),
        getCameraToggleSublabel()!.textContent!.trim());

    assertEquals(null, getMicrophoneList());
    assert(getNoMicrophoneText());
    assertEquals(
        privacyHubSubpage.i18n('noMicrophoneConnectedText'),
        getNoMicrophoneText()!.textContent!.trim());
    assertEquals(
        privacyHubSubpage.i18n('microphoneToggleSubtext'),
        getMicrophoneToggleSublabel()!.textContent!.trim());

    const tests = [
      {
        device: {
          kind: 'audiooutput',
          label: 'Fake Speaker 1',
        },
        changes: {
          cam: false,
          mic: false,
        },
      },
      {
        device: {
          kind: 'videoinput',
          label: 'Fake Camera 1',
        },
        changes: {
          mic: false,
          cam: true,
        },
      },
      {
        device: {
          kind: 'audioinput',
          label: 'Fake Microphone 1',
        },
        changes: {
          cam: false,
          mic: true,
        },
      },
      {
        device: {
          kind: 'videoinput',
          label: 'Fake Camera 2',
        },
        changes: {
          cam: true,
          mic: false,
        },
      },
      {
        device: {
          kind: 'audiooutput',
          label: 'Fake Speaker 2',
        },
        changes: {
          cam: false,
          mic: false,
        },
      },
      {
        device: {
          kind: 'audioinput',
          label: 'Fake Microphone 2',
        },
        changes: {
          cam: false,
          mic: true,
        },
      },
    ];

    let cams = 0;
    let mics = 0;

    // Adding a media device in each iteration.
    for (const test of tests) {
      mediaDevices.addDevice(test.device.kind, test.device.label);
      await waitAfterNextRender(privacyHubSubpage);

      if (test.changes.cam) {
        cams++;
      }
      if (test.changes.mic) {
        mics++;
      }

      const cameraList = getCameraList();
      if (cams) {
        assert(cameraList);
        assertEquals(cams, cameraList.items!.length);
        assertFalse(getCameraCrToggle()!.disabled);
      } else {
        assertEquals(null, cameraList);
        assertTrue(getCameraCrToggle()!.disabled);
      }

      const microphoneList = getMicrophoneList();
      if (mics) {
        assert(microphoneList);
        assertEquals(mics, microphoneList.items!.length);
        assertFalse(getMicrophoneCrToggle()!.disabled);
      } else {
        assertEquals(null, microphoneList);
        assertTrue(getMicrophoneCrToggle()!.disabled);
      }
    }

    // Removing the most recently added media device in each iteration.
    for (const test of tests.reverse()) {
      mediaDevices.popDevice();
      await waitAfterNextRender(privacyHubSubpage);

      if (test.changes.cam) {
        cams--;
      }
      if (test.changes.mic) {
        mics--;
      }

      const cameraList = getCameraList();
      if (cams) {
        assert(cameraList);
        assertEquals(cams, cameraList.items!.length);
        assertFalse(getCameraCrToggle()!.disabled);
      } else {
        assertEquals(null, cameraList);
        assertTrue(getCameraCrToggle()!.disabled);
      }

      const microphoneList = getMicrophoneList();
      if (mics) {
        assert(microphoneList);
        assertEquals(mics, microphoneList.items!.length);
        assertFalse(getMicrophoneCrToggle()!.disabled);
      } else {
        assertEquals(null, microphoneList);
        assertTrue(getMicrophoneCrToggle()!.disabled);
      }
    }
  });

  test('Toggle camera button', async () => {
    privacyHubSubpage.remove();
    privacyHubSubpage = document.createElement('settings-privacy-hub-subpage');
    privacyHubSubpage.prefs = {
      'ash': {
        'user': {
          'camera_allowed': {
            value: true,
          },
        },
      },
    };
    document.body.appendChild(privacyHubSubpage);
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate =
        fakeMetricsPrivate as unknown as typeof chrome.metricsPrivate;
    flush();

    mediaDevices.addDevice('videoinput', 'Fake Camera');

    await waitAfterNextRender(privacyHubSubpage);

    const cameraToggleControl =
        privacyHubSubpage.shadowRoot!.querySelector('#cameraToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(cameraToggleControl);

    // Pref and toggle should be in sync and not disabled
    assertTrue(cameraToggleControl.checked);
    assertTrue(privacyHubSubpage.prefs.ash.user.camera_allowed.value);
    assertFalse(cameraToggleControl.disabled);

    // Click the button
    cameraToggleControl.click();
    flush();

    await waitAfterNextRender(cameraToggleControl);

    assertFalse(privacyHubSubpage.prefs.ash.user.camera_allowed.value);
    assertFalse(cameraToggleControl.checked);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Camera.Settings.Enabled', false),
        1);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Camera.Settings.Enabled', true),
        0);

    // Click the button again
    cameraToggleControl.click();
    flush();

    await waitAfterNextRender(cameraToggleControl);

    assertTrue(privacyHubSubpage.prefs.ash.user.camera_allowed.value);
    assertTrue(cameraToggleControl.checked);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Camera.Settings.Enabled', false),
        1);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Camera.Settings.Enabled', true),
        1);
  });

  test('Toggle microphone button', async () => {
    privacyHubSubpage.remove();
    privacyHubSubpage = document.createElement('settings-privacy-hub-subpage');
    privacyHubSubpage.prefs = {
      'ash': {
        'user': {
          'microphone_allowed': {
            value: true,
          },
        },
      },
    };

    document.body.appendChild(privacyHubSubpage);
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate =
        fakeMetricsPrivate as unknown as typeof chrome.metricsPrivate;
    flush();

    mediaDevices.addDevice('audioinput', 'Fake Mic');

    await waitAfterNextRender(privacyHubSubpage);

    const microphoneToggleControl =
        privacyHubSubpage.shadowRoot!.querySelector('#microphoneToggle')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(microphoneToggleControl);

    // Pref and toggle should be in sync and not disabled
    assertTrue(microphoneToggleControl.checked);
    assertTrue(privacyHubSubpage.prefs.ash.user.microphone_allowed.value);
    assertFalse(microphoneToggleControl.disabled);

    // Click the button
    microphoneToggleControl.click();
    flush();

    await waitAfterNextRender(microphoneToggleControl);

    assertFalse(privacyHubSubpage.prefs.ash.user.microphone_allowed.value);
    assertFalse(microphoneToggleControl.checked);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Microphone.Settings.Enabled', false),
        1);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Microphone.Settings.Enabled', true),
        0);

    // Click the button again
    microphoneToggleControl.click();
    flush();

    await waitAfterNextRender(microphoneToggleControl);

    assertTrue(privacyHubSubpage.prefs.ash.user.microphone_allowed.value);
    assertTrue(microphoneToggleControl.checked);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Microphone.Settings.Enabled', false),
        1);
    assertEquals(
        fakeMetricsPrivate.countBoolean(
            'ChromeOS.PrivacyHub.Microphone.Settings.Enabled', true),
        1);
  });
}

suite(
    '<settings-privacy-hub-subpage> Dogfood',
    () => parametrizedPrivacyHubSubpageTestsuite(PrivacyHubVersion.Dogfood));
suite(
    '<settings-privacy-hub-subpage> MVP',
    () => parametrizedPrivacyHubSubpageTestsuite(PrivacyHubVersion.MVP));
suite(
    '<settings-privacy-hub-subpage> Future',
    () => parametrizedPrivacyHubSubpageTestsuite(PrivacyHubVersion.Future));

async function parametrizedTestsuiteForMetricsConsentToggle(
    isPrivacyHubVisible: boolean) {
  let settingsPage: SettingsPrivacyHubSubpage|OsSettingsPrivacyPageElement;

  // Which settings page to run the tests on.
  const pageId = isPrivacyHubVisible ? 'settings-privacy-hub-subpage' :
                                       'os-settings-privacy-page';

  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: true,
        },
      },
      'metrics': {
        'reportingEnabled': {
          value: true,
        },
      },
    },
    'metrics': {
      'user_consent': {
        value: false,
      },
    },
    'dns_over_https':
        {'mode': {value: SecureDnsMode.AUTOMATIC}, 'templates': {value: ''}},
  };

  let metricsConsentBrowserProxy: TestMetricsConsentBrowserProxy;

  setup(async () => {
    loadTimeData.overrideValues({
      showPrivacyHubPage: isPrivacyHubVisible,
    });
    metricsConsentBrowserProxy = new TestMetricsConsentBrowserProxy();
    MetricsConsentBrowserProxyImpl.setInstanceForTesting(
        metricsConsentBrowserProxy);

    settingsPage = document.createElement(pageId);
  });

  async function setUpPage(prefName: string, isConfigurable: boolean) {
    metricsConsentBrowserProxy.setMetricsConsentState(prefName, isConfigurable);

    settingsPage = document.createElement(pageId);
    settingsPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(settingsPage);
    flush();

    await metricsConsentBrowserProxy.whenCalled('getMetricsConsentState');
    await waitAfterNextRender(settingsPage);
    flush();
  }

  teardown(() => {
    settingsPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'Send usage stats toggle visibility in os-settings-privacy-page',
      async () => {
        settingsPage = document.createElement('os-settings-privacy-page');
        document.body.appendChild(settingsPage);
        flush();

        const element =
            settingsPage.shadowRoot!.querySelector('#metricsConsentToggle');

        assertEquals(
            isPrivacyHubVisible, element === null,
            'Send usage toggle should only be visible here when privacy hub' +
                ' is hidden.');
      });

  test(
      'Send usage stats toggle visibility in settings-privacy-hub-subpage',
      async () => {
        if (isPrivacyHubVisible) {
          settingsPage = document.createElement('settings-privacy-hub-subpage');
          document.body.appendChild(settingsPage);
          flush();

          const element =
              settingsPage.shadowRoot!.querySelector('#metricsConsentToggle');

          assertFalse(
              element === null,
              'Send usage toggle should be visible in the privacy hub' +
                  ' subpage.');
        }
      });

  test('Deep link to send usage stats', async () => {
    await setUpPage(DEVICE_METRICS_CONSENT_PREF_NAME, /*isConfigurable=*/ true);

    const params = new URLSearchParams();
    params.append('settingId', '1103');
    Router.getInstance().navigateTo(
        isPrivacyHubVisible ? routes.PRIVACY_HUB : routes.OS_PRIVACY, params);

    flush();

    const deepLinkElement =
        settingsPage.shadowRoot!.querySelector(
                                    '#metricsConsentToggle')!.shadowRoot!
            .querySelector('#settingsToggle')!.shadowRoot!.querySelector(
                'cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);

    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Send usage stats toggle should be focused for settingId=1103.');
  });

  test('Toggle disabled if metrics consent is not configurable', async () => {
    await setUpPage(
        DEVICE_METRICS_CONSENT_PREF_NAME, /*isConfigurable=*/ false);

    const toggle =
        settingsPage.shadowRoot!.querySelector(
                                    '#metricsConsentToggle')!.shadowRoot!
            .querySelector('#settingsToggle')!.shadowRoot!.querySelector(
                'cr-toggle');
    assert(toggle);
    await waitAfterNextRender(toggle);

    // The pref is true, so the toggle should be on.
    assertTrue(toggle.checked);

    // Not configurable, so toggle should be disabled.
    assertTrue(toggle.disabled);
  });

  test('Toggle enabled if metrics consent is configurable', async () => {
    await setUpPage(DEVICE_METRICS_CONSENT_PREF_NAME, /*isConfigurable=*/ true);

    const toggle =
        settingsPage.shadowRoot!.querySelector(
                                    '#metricsConsentToggle')!.shadowRoot!
            .querySelector('#settingsToggle')!.shadowRoot!.querySelector(
                'cr-toggle');
    assert(toggle);
    await waitAfterNextRender(toggle);

    // The pref is true, so the toggle should be on.
    assertTrue(toggle.checked);

    // Configurable, so toggle should be enabled.
    assertFalse(toggle.disabled);

    // Toggle.
    toggle.click();
    await metricsConsentBrowserProxy.whenCalled('updateMetricsConsent');

    // Pref should be off now.
    assertFalse(toggle.checked);
  });

  test('Correct pref displayed', async () => {
    await setUpPage(USER_METRICS_CONSENT_PREF_NAME, /*isConfigurable=*/ true);

    const toggle =
        settingsPage.shadowRoot!.querySelector(
                                    '#metricsConsentToggle')!.shadowRoot!
            .querySelector('#settingsToggle')!.shadowRoot!.querySelector(
                'cr-toggle');
    assert(toggle);
    await waitAfterNextRender(toggle);

    // The user consent pref is false, so the toggle should not be checked.
    assertFalse(toggle.checked);

    // Configurable, so toggle should be enabled.
    assertFalse(toggle.disabled);

    // Toggle.
    toggle.click();
    await metricsConsentBrowserProxy.whenCalled('updateMetricsConsent');

    // Pref should be on now.
    assertTrue(toggle.checked);
  });
}

suite(
    '<os-settings-privacy-page> OfficialBuild PrivacyHubVisible',
    () => parametrizedTestsuiteForMetricsConsentToggle(
        /*isPrivacyHubVisible=*/ true));

suite(
    '<os-settings-privacy-page> OfficialBuild PrivacyHubHidden',
    () => parametrizedTestsuiteForMetricsConsentToggle(
        /*isPrivacyHubVisible=*/ false));
