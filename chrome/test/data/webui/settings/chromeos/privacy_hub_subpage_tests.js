// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../chai.js';

import {MediaDevicesProxy, PrivacyHubBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {MetricsConsentBrowserProxyImpl, Router, routes, SecureDnsMode} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertNotReached, assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

import {FakeMediaDevices} from './fake_media_devices.js';
import {DEVICE_METRICS_CONSENT_PREF_NAME, TestMetricsConsentBrowserProxy} from './test_metrics_consent_browser_proxy.js';

const USER_METRICS_CONSENT_PREF_NAME = 'metrics.user_consent';

/** @implements {PrivacyHubBrowserProxy} */
class TestPrivacyHubBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getInitialCameraHardwareToggleState',
      'getInitialMicrophoneHardwareToggleState',
      'getInitialAvailabilityOfMicrophoneForSimpleUsage',
    ]);
    this.cameraToggleIsEnabled = false;
    this.microphoneToggleIsEnabled = false;
    this.microphoneForSimpleUsageAvailable = false;
  }

  /** override */
  getInitialCameraHardwareToggleState() {
    this.methodCalled('getInitialCameraHardwareToggleState');
    return Promise.resolve(this.cameraToggleIsEnabled);
  }

  /** override */
  getInitialMicrophoneHardwareToggleState() {
    this.methodCalled('getInitialMicrophoneHardwareToggleState');
    return Promise.resolve(this.microphoneToggleIsEnabled);
  }

  /** override */
  getInitialAvailabilityOfMicrophoneForSimpleUsage() {
    this.methodCalled('getInitialAvailabilityOfMicrophoneForSimpleUsage');
    return Promise.resolve(this.microphoneForSimpleUsageAvailable);
  }
}

const PrivacyHubVersion = {
  Future: 'Privacy Hub with future (after MVP) features.',
  MVP: 'Privacy Hub with MVP features.',
  Dogfood: 'Privacy Hub with dogfooded features (camera and microphone only).',
};

function overridedValues(privacyHubVersion) {
  switch (privacyHubVersion) {
    case PrivacyHubVersion.Future: {
      return {
        showPrivacyHubPage: true,
        showPrivacyHubMVPPage: true,
        showPrivacyHubFuturePage: true,
      };
    }
    case PrivacyHubVersion.Dogfood: {
      return {
        showPrivacyHubPage: true,
        showPrivacyHubMVPPage: false,
        showPrivacyHubFuturePage: false,
      };
    }
    case PrivacyHubVersion.MVP: {
      return {
        showPrivacyHubPage: true,
        showPrivacyHubMVPPage: true,
        showPrivacyHubFuturePage: false,
      };
    }
    default: {
      assertNotReached(`Unsupported Privacy Hub version: {privacyHubVersion}`);
    }
  }
}

async function parametrizedPrivacyHubSubpageTestsuite(privacyHubVersion) {
  /** @type {SettingsPrivacyHubPage} */
  let privacyHubSubpage = null;

  /** @type {?TestPrivacyHubBrowserProxy} */
  let privacyHubBrowserProxy = null;

  /** @type {?FakeMediaDevices} */
  let mediaDevices = null;

  setup(async () => {
    loadTimeData.overrideValues(overridedValues(privacyHubVersion));

    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);
    privacyHubBrowserProxy.resetResolver('getInitialCameraHardwareToggleState');
    mediaDevices = new FakeMediaDevices();
    MediaDevicesProxy.setMediaDevicesForTesting(mediaDevices);


    PolymerTest.clearBody();
    privacyHubSubpage = document.createElement('settings-privacy-hub-page');
    document.body.appendChild(privacyHubSubpage);
    await waitAfterNextRender(privacyHubSubpage);
    flush();
  });

  teardown(function() {
    privacyHubSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to camera toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1116');

    await privacyHubBrowserProxy.whenCalled(
        'getInitialCameraHardwareToggleState');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot.querySelector('#cameraToggle')
            .shadowRoot.querySelector('cr-toggle');
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
        privacyHubSubpage.shadowRoot.querySelector('#microphoneToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Microphone toggle should be focused for settingId=1117.');
  });


  test('Update camera setting sub-label', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1116');

    privacyHubBrowserProxy.cameraToggleIsEnabled = false;

    await privacyHubBrowserProxy.whenCalled(
        'getInitialCameraHardwareToggleState');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const subLabel = privacyHubSubpage.shadowRoot.querySelector('#cameraToggle')
                         .shadowRoot.querySelector('#sub-label-text');

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*$/,
        'The sublabel should only consist of whitespace');

    webUIListenerCallback('camera-hardware-toggle-changed', true);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent,
        /^\s*Internal camera deactivated by hardware switch\s*$/,
        'The sublabel should contain the hint about the internal camera');

    webUIListenerCallback('camera-hardware-toggle-changed', false);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*$/,
        'The sublabel should only consist of whitespace');
  });

  test('Update microphone setting sub-label', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1117');

    privacyHubBrowserProxy.microphoneToggleIsEnabled = false;
    privacyHubBrowserProxy.microphoneForSimpleUsageAvailable = false;

    await privacyHubBrowserProxy.whenCalled(
        'getInitialMicrophoneHardwareToggleState');
    await privacyHubBrowserProxy.whenCalled(
        'getInitialAvailabilityOfMicrophoneForSimpleUsage');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const subLabel =
        privacyHubSubpage.shadowRoot.querySelector('#microphoneToggle')
            .shadowRoot.querySelector('#sub-label-text');

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*No microphone connected\s*$/,
        'The sublabel should contain the hint about no microphone being ' +
            'connected.');

    webUIListenerCallback(
        'availability-of-microphone-for-simple-usage-changed', true);
    webUIListenerCallback('microphone-hardware-toggle-changed', true);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent,
        /^\s*All microphones disabled by devices hardware switch\s*$/,
        'The sublabel should contain the hint about the microphone hardware ' +
            'switch being active.');


    webUIListenerCallback(
        'availability-of-microphone-for-simple-usage-changed', true);
    webUIListenerCallback('microphone-hardware-toggle-changed', false);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent, /^\s*$/,
        'The sublabel should only consist of whitespace');

    webUIListenerCallback(
        'availability-of-microphone-for-simple-usage-changed', false);
    webUIListenerCallback('microphone-hardware-toggle-changed', true);

    await waitAfterNextRender(subLabel);

    chai.assert.match(
        subLabel.textContent,
        /^\s*All microphones disabled by devices hardware switch\s*$/,
        'The sublabel should contain the hint about the microphone hardware ' +
            'switch being active.');
  });

  test('Suggested content, pref disabled', async () => {
    privacyHubSubpage = document.createElement('settings-privacy-hub-page');
    document.body.appendChild(privacyHubSubpage);
    flush();

    // The default state of the pref is disabled.
    const suggestedContent = assert(
        privacyHubSubpage.shadowRoot.querySelector('#suggested-content'));
    assertFalse(suggestedContent.checked);
  });

  test('Suggested content, pref enabled', async () => {
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
    const suggestedContent = assert(
        privacyHubSubpage.shadowRoot.querySelector('#suggested-content'));
    assertTrue(suggestedContent.checked);
  });

  test('Deep link to Geolocation toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1118');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const toggleElement =
        privacyHubSubpage.shadowRoot.querySelector('#geolocationToggle');
    if (privacyHubVersion === PrivacyHubVersion.Dogfood) {
      assertEquals(toggleElement, null);
    } else {
      const deepLinkElement =
          toggleElement.shadowRoot.querySelector('cr-toggle');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Geolocation toggle should be focused for settingId=1118.');
    }
  });

  test('Media device lists in the Privacy Hub subpage', async () => {
    const getNoCameraText = () =>
        privacyHubSubpage.shadowRoot.querySelector('#noCamera');
    const getCameraList = () =>
        privacyHubSubpage.shadowRoot.querySelector('#cameraList');

    const getNoMicrophoneText = () =>
        privacyHubSubpage.shadowRoot.querySelector('#noMic');
    const getMicrophoneList = () =>
        privacyHubSubpage.shadowRoot.querySelector('#micList');

    const getCameraCrToggle = () =>
        privacyHubSubpage.shadowRoot.querySelector('#cameraToggle')
            .shadowRoot.querySelector('cr-toggle');

    // Initially, the lists of media devices should be hidden and `#noMic` and
    // `#noCamera` should be displayed.
    assertFalse(!!getCameraList());
    assertTrue(!!getNoCameraText());
    assertEquals(
        getNoCameraText().textContent.trim(),
        privacyHubSubpage.i18n('noCameraConnectedText'));

    assertFalse(!!getMicrophoneList());
    assertTrue(!!getNoMicrophoneText());
    assertEquals(
        getNoMicrophoneText().textContent.trim(),
        privacyHubSubpage.i18n('noMicrophoneConnectedText'));

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

      if (cams) {
        assertTrue(!!getCameraList());
        assertEquals(getCameraList().items.length, cams);
        assertFalse(getCameraCrToggle().disabled);
      } else {
        assertFalse(!!getCameraList());
        assertTrue(getCameraCrToggle().disabled);
      }

      if (mics) {
        assertTrue(!!getMicrophoneList());
        assertEquals(getMicrophoneList().items.length, mics);
      } else {
        assertFalse(!!getMicrophoneList());
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

      if (cams) {
        assertTrue(!!getCameraList());
        assertEquals(getCameraList().items.length, cams);
        assertFalse(getCameraCrToggle().disabled);
      } else {
        assertFalse(!!getCameraList());
        assertTrue(getCameraCrToggle().disabled);
      }

      if (mics) {
        assertTrue(!!getMicrophoneList());
        assertEquals(getMicrophoneList().items.length, mics);
      } else {
        assertFalse(!!getMicrophoneList());
      }
    }
  });
}

suite(
    'PrivacyHubDogfoodSubpageTests',
    () => parametrizedPrivacyHubSubpageTestsuite(PrivacyHubVersion.Dogfood));
suite(
    'PrivacyHubMVPSubpageTests',
    () => parametrizedPrivacyHubSubpageTestsuite(PrivacyHubVersion.MVP));
suite(
    'PrivacyHubFutureSubpageTests',
    () => parametrizedPrivacyHubSubpageTestsuite(PrivacyHubVersion.Future));

async function parametrizedTestsuiteForMetricsConsentToggle(
    isPrivacyHubVisible) {
  /** @type {SettingsPrivacyPageElement} */
  let settingsPage = null;

  // Which settings page to run the tests on.
  const pageId = isPrivacyHubVisible ? 'settings-privacy-hub-page' :
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

  /** @type {?TestMetricsConsentBrowserProxy} */
  let metricsConsentBrowserProxy = null;

  setup(async () => {
    loadTimeData.overrideValues({
      showPrivacyHubPage: isPrivacyHubVisible,
    });
    metricsConsentBrowserProxy = new TestMetricsConsentBrowserProxy();
    MetricsConsentBrowserProxyImpl.setInstanceForTesting(
        metricsConsentBrowserProxy);

    settingsPage = document.createElement(pageId);
    PolymerTest.clearBody();
  });

  async function setUpPage(prefName, isConfigurable) {
    metricsConsentBrowserProxy.setMetricsConsentState(prefName, isConfigurable);

    settingsPage = document.createElement(pageId);
    settingsPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(settingsPage);
    flush();

    await metricsConsentBrowserProxy.whenCalled('getMetricsConsentState');
    await waitAfterNextRender();
    flush();
  }

  teardown(function() {
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
            settingsPage.shadowRoot.querySelector('#metricsConsentToggle');

        assertEquals(
            element === null, isPrivacyHubVisible,
            'Send usage toggle should only be visible here when privacy hub' +
                ' is hidden.');
      });

  test(
      'Send usage stats toggle visibility in settings-privacy-hub-page',
      async () => {
        if (isPrivacyHubVisible) {
          settingsPage = document.createElement('settings-privacy-hub-page');
          document.body.appendChild(settingsPage);
          flush();

          const element =
              settingsPage.shadowRoot.querySelector('#metricsConsentToggle');

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
        settingsPage.shadowRoot.querySelector('#metricsConsentToggle')
            .shadowRoot.querySelector('#settingsToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);

    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Send usage stats toggle should be focused for settingId=1103.');
  });

  test('Toggle disabled if metrics consent is not configurable', async () => {
    await setUpPage(
        DEVICE_METRICS_CONSENT_PREF_NAME, /*isConfigurable=*/ false);

    const toggle =
        settingsPage.shadowRoot.querySelector('#metricsConsentToggle')
            .shadowRoot.querySelector('#settingsToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(toggle);

    // The pref is true, so the toggle should be on.
    assertTrue(toggle.checked);

    // Not configurable, so toggle should be disabled.
    assertTrue(toggle.disabled);
  });

  test('Toggle enabled if metrics consent is configurable', async () => {
    await setUpPage(DEVICE_METRICS_CONSENT_PREF_NAME, /*isConfigurable=*/ true);

    const toggle =
        settingsPage.shadowRoot.querySelector('#metricsConsentToggle')
            .shadowRoot.querySelector('#settingsToggle')
            .shadowRoot.querySelector('cr-toggle');
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
        settingsPage.shadowRoot.querySelector('#metricsConsentToggle')
            .shadowRoot.querySelector('#settingsToggle')
            .shadowRoot.querySelector('cr-toggle');
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
    'PrivacyHubSubpageTest_OfficialBuild',
    () => parametrizedTestsuiteForMetricsConsentToggle(
        /*isPrivacyHubVisible=*/ true));

suite(
    'PrivacyHubSubpageTest_OfficialBuild',
    () => parametrizedTestsuiteForMetricsConsentToggle(
        /*isPrivacyHubVisible=*/ false));
