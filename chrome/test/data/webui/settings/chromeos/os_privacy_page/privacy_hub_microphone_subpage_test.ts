// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MediaDevicesProxy, PrivacyHubBrowserProxyImpl, SettingsPrivacyHubMicrophoneSubpage} from 'chrome://os-settings/lazy_load.js';
import {appPermissionHandlerMojom, CrLinkRowElement, CrToggleElement, PaperTooltipElement, PrivacyHubSensorSubpageUserAction, Router, setAppPermissionProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMediaDevices} from '../fake_media_devices.js';
import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';
import {createApp, createFakeMetricsPrivate, getSystemServicePermissionText, getSystemServicesFromSubpage} from './privacy_hub_app_permission_test_util.js';
import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

type App = appPermissionHandlerMojom.App;

suite('<settings-privacy-hub-microphone-subpage>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let metrics: FakeMetricsPrivate;
  let privacyHubMicrophoneSubpage: SettingsPrivacyHubMicrophoneSubpage;
  let privacyHubBrowserProxy: TestPrivacyHubBrowserProxy;
  let mediaDevices: FakeMediaDevices;

  setup(() => {
    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);

    metrics = createFakeMetricsPrivate();

    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    mediaDevices = new FakeMediaDevices();
    MediaDevicesProxy.setMediaDevicesForTesting(mediaDevices);

    privacyHubMicrophoneSubpage =
        document.createElement('settings-privacy-hub-microphone-subpage');
    const prefs = {
      'ash': {
        'user': {
          'microphone_allowed': {
            value: true,
          },
        },
      },
    };
    privacyHubMicrophoneSubpage.prefs = prefs;
    document.body.appendChild(privacyHubMicrophoneSubpage);
    flush();
  });

  teardown(() => {
    privacyHubMicrophoneSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function getMicrophoneCrToggle(): CrToggleElement {
    const crToggle =
        privacyHubMicrophoneSubpage.shadowRoot!.querySelector<CrToggleElement>(
            '#microphoneToggle');
    assertTrue(!!crToggle);
    return crToggle;
  }

  function getMicrophoneTooltip(): PaperTooltipElement {
    const tooltip =
        privacyHubMicrophoneSubpage.shadowRoot!
            .querySelector<PaperTooltipElement>('#microphoneToggleTooltip');
    assertTrue(!!tooltip);
    return tooltip;
  }

  function getOnOffText(): string {
    return privacyHubMicrophoneSubpage.shadowRoot!.querySelector('#onOffText')!
        .textContent!.trim();
  }

  function getOnOffSubtext(): string {
    return privacyHubMicrophoneSubpage.shadowRoot!
        .querySelector('#onOffSubtext')!.textContent!.trim();
  }

  function isMicrophoneListSectionVisible(): boolean {
    return isVisible(privacyHubMicrophoneSubpage.shadowRoot!.querySelector(
        '#microphoneListSection'));
  }

  function getNoMicrophoneTextElement(): HTMLDivElement|null {
    return privacyHubMicrophoneSubpage.shadowRoot!.querySelector(
        '#noMicrophoneText');
  }

  function getMicrophoneList(): DomRepeat|null {
    return privacyHubMicrophoneSubpage.shadowRoot!.querySelector<DomRepeat>(
        '#microphoneList');
  }

  test('Microphone access is allowed by default', () => {
    assertTrue(getMicrophoneCrToggle().checked);
  });

  function isBlockedSuffixDisplayedAfterMicrophoneName(): boolean {
    return isVisible(privacyHubMicrophoneSubpage.shadowRoot!.querySelector(
        '#microphoneNameWithBlockedSuffix'));
  }

  test('Microphone section view when microphone access is enabled', () => {
    const microphoneToggle = getMicrophoneCrToggle();

    assertEquals(
        microphoneToggle.checked,
        privacyHubMicrophoneSubpage.prefs.ash.user.microphone_allowed.value);
    assertEquals(privacyHubMicrophoneSubpage.i18n('deviceOn'), getOnOffText());
    assertEquals(
        privacyHubMicrophoneSubpage.i18n(
            'privacyHubMicrophoneSubpageMicrophoneToggleSubtext'),
        getOnOffSubtext());
    assertTrue(isMicrophoneListSectionVisible());
    assertFalse(isBlockedSuffixDisplayedAfterMicrophoneName());
  });

  test(
      'Microphone section view when microphone access is disabled',
      async () => {
        // Adding a microphone, otherwise clicking on the toggle will be no-op.
        mediaDevices.addDevice('audioinput', 'Fake Microphone');
        await waitAfterNextRender(privacyHubMicrophoneSubpage);

        const microphoneToggle = getMicrophoneCrToggle();

        // Disabled microphone access.
        microphoneToggle.click();
        await waitAfterNextRender(privacyHubMicrophoneSubpage);

        assertEquals(
            microphoneToggle.checked,
            privacyHubMicrophoneSubpage.prefs.ash.user.microphone_allowed
                .value);
        assertEquals(
            privacyHubMicrophoneSubpage.i18n('deviceOff'), getOnOffText());
        assertEquals(
            privacyHubMicrophoneSubpage.i18n(
                'privacyHubMicrophoneAccessBlockedText'),
            getOnOffSubtext());
        assertTrue(isMicrophoneListSectionVisible());
        assertTrue(isBlockedSuffixDisplayedAfterMicrophoneName());
        assertEquals(
            privacyHubMicrophoneSubpage.i18n(
                'privacyHubSensorNameWithBlockedSuffix', 'Fake Microphone'),
            privacyHubMicrophoneSubpage.shadowRoot!
                .querySelector<HTMLDivElement>(
                    '#microphoneNameWithBlockedSuffix')!.innerText.trim());
      });

  test('Repeatedly toggle microphone access', async () => {
    // Adding a microphone, otherwise clicking on the toggle will be no-op.
    mediaDevices.addDevice('audioinput', 'Fake Microphone');
    await waitAfterNextRender(privacyHubMicrophoneSubpage);

    const microphoneToggle = getMicrophoneCrToggle();

    for (let i = 0; i < 3; i++) {
      // Toggle microphone access.
      microphoneToggle.click();
      await waitAfterNextRender(privacyHubMicrophoneSubpage);

      assertEquals(
          microphoneToggle.checked,
          privacyHubMicrophoneSubpage.prefs.ash.user.microphone_allowed.value);
      assertEquals(
          i + 1,
          metrics.countMetricValue(
              'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
              PrivacyHubSensorSubpageUserAction.SYSTEM_ACCESS_CHANGED));
    }
  });

  function isMicrophoneRowActionable(): boolean {
    const actionableAttribute =
        privacyHubMicrophoneSubpage.shadowRoot!
            .querySelector('#accessStatusRow')!.getAttribute('actionable');
    return actionableAttribute === '';
  }

  test(
      'Clicking toggle is no-op when accessStatusRow is not actionable',
      async () => {
        const microphoneToggle = getMicrophoneCrToggle();
        assertTrue(microphoneToggle.checked);

        assertFalse(isMicrophoneRowActionable());
        microphoneToggle.click();
        assertTrue(microphoneToggle.checked);

        // Add a microphone to make accessStatusRow actionable.
        mediaDevices.addDevice('audioinput', 'Fake Microphone');
        await flushTasks();
        assertTrue(isMicrophoneRowActionable());
        microphoneToggle.click();
        assertFalse(microphoneToggle.checked);

        // Activate microphone hardware toggle to make the accessStatusRow not
        // actionable.
        webUIListenerCallback('microphone-hardware-toggle-changed', true);
        flush();
        assertFalse(isMicrophoneRowActionable());
        microphoneToggle.click();
        assertFalse(microphoneToggle.checked);
      });

  test('No microphone connected by default', () => {
    assertNull(getMicrophoneList());
    assertTrue(!!getNoMicrophoneTextElement());
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('noMicrophoneConnectedText'),
        getNoMicrophoneTextElement()!.textContent!.trim());
  });

  test(
      'Toggle disabled but no tooltip displayed when no microphone connected',
      () => {
        assertTrue(getMicrophoneCrToggle()!.disabled);
        assertTrue(getMicrophoneTooltip()!.hidden);
      });

  test(
      'Toggle enabled when at least one microphone connected but no tooltip',
      async () => {
        // Add a microphone.
        mediaDevices.addDevice('audioinput', 'Fake Microphone');
        await waitAfterNextRender(privacyHubMicrophoneSubpage);

        assertFalse(getMicrophoneCrToggle()!.disabled);
        assertTrue(getMicrophoneTooltip()!.hidden);
        assertNull(getNoMicrophoneTextElement());
      });

  test(
      'Toggle disabled and a tooltip displayed when hardware switch is active',
      async () => {
        // Add a microphone.
        mediaDevices.addDevice('audioinput', 'Fake Microphone');
        await waitAfterNextRender(privacyHubMicrophoneSubpage);

        assertFalse(getMicrophoneCrToggle()!.disabled);
        assertTrue(getMicrophoneTooltip()!.hidden);

        // Activate the hw toggle.
        webUIListenerCallback('microphone-hardware-toggle-changed', true);
        await waitAfterNextRender(privacyHubMicrophoneSubpage);

        assertTrue(getMicrophoneCrToggle()!.disabled);
        assertFalse(getMicrophoneTooltip()!.hidden);
      });

  test(
      'Microphone list updated when a microphone is added or removed',
      async () => {
        const testDevices = [
          {
            device: {
              kind: 'audiooutput',
              label: 'Fake Speaker 1',
            },
          },
          {
            device: {
              kind: 'videoinput',
              label: 'Fake Camera 1',
            },
          },
          {
            device: {
              kind: 'audioinput',
              label: 'Fake Microphone 1',
            },
          },
          {
            device: {
              kind: 'videoinput',
              label: 'Fake Camera 2',
            },
          },
          {
            device: {
              kind: 'audiooutput',
              label: 'Fake Speaker 2',
            },
          },
          {
            device: {
              kind: 'audioinput',
              label: 'Fake Microphone 2',
            },
          },
        ];

        let microphoneCount = 0;

        // Adding a media device in each iteration.
        for (const test of testDevices) {
          mediaDevices.addDevice(test.device.kind, test.device.label);
          await waitAfterNextRender(privacyHubMicrophoneSubpage);

          if (test.device.kind === 'audioinput') {
            microphoneCount++;
          }

          const microphoneList = getMicrophoneList();
          if (microphoneCount) {
            assertTrue(!!microphoneList);
            assertEquals(microphoneCount, microphoneList.items!.length);
          } else {
            assertNull(microphoneList);
          }
        }

        // Removing the most recently added media device in each iteration.
        for (const test of testDevices.reverse()) {
          mediaDevices.popDevice();
          await waitAfterNextRender(privacyHubMicrophoneSubpage);

          if (test.device.kind === 'audioinput') {
            microphoneCount--;
          }

          const microphoneList = getMicrophoneList();
          if (microphoneCount) {
            assertTrue(!!microphoneList);
            assertEquals(microphoneCount, microphoneList.items!.length);
          } else {
            assertNull(microphoneList);
          }
        }
      });

  function getNoAppHasAccessTextSection(): HTMLDivElement|null {
    return privacyHubMicrophoneSubpage.shadowRoot!.querySelector(
        '#noAppHasAccessText');
  }

  function getAppList(): DomRepeat|null {
    return privacyHubMicrophoneSubpage.shadowRoot!.querySelector('#appList');
  }

  test('Apps section when microphone allowed', () => {
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('privacyHubAppsSectionTitle'),
        privacyHubMicrophoneSubpage.shadowRoot!
            .querySelector('#appsSectionTitle')!.textContent!.trim());
    assertTrue(!!getAppList());
    assertNull(getNoAppHasAccessTextSection());
  });

  test('Apps section when microphone not allowed', async () => {
    mediaDevices.addDevice('audioinput', 'Fake Microphone');
    await waitAfterNextRender(privacyHubMicrophoneSubpage);
    // Disable microphone access.
    getMicrophoneCrToggle().click();
    await waitAfterNextRender(privacyHubMicrophoneSubpage);

    assertNull(getAppList());
    assertTrue(!!getNoAppHasAccessTextSection());
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('noAppCanUseMicText'),
        getNoAppHasAccessTextSection()!.textContent!.trim());
  });

  function initializeObserver(): Promise<void> {
    return fakeHandler.whenCalled('addObserver');
  }

  function simulateAppUpdate(app: App): void {
    fakeHandler.getObserverRemote().onAppUpdated(app);
  }

  function simulateAppRemoval(id: string): void {
    fakeHandler.getObserverRemote().onAppRemoved(id);
  }

  test('AppList displays all apps with microphone permission', async () => {
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kMicrophone, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kCamera, TriState.kAllow);
    const app3 = createApp(
        'app3_id', 'app3_name', PermissionType.kMicrophone, TriState.kAsk);

    await initializeObserver();
    simulateAppUpdate(app1);
    simulateAppUpdate(app2);
    simulateAppUpdate(app3);
    await flushTasks();

    assertEquals(2, getAppList()!.items!.length);
  });

  test('Removed app are removed from appList', async () => {
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kMicrophone, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kCamera, TriState.kAllow);

    await initializeObserver();
    simulateAppUpdate(app1);
    simulateAppUpdate(app2);
    await flushTasks();

    assertEquals(1, getAppList()!.items!.length);

    simulateAppRemoval(app2.id);
    await flushTasks();

    assertEquals(1, getAppList()!.items!.length);

    simulateAppRemoval(app1.id);
    await flushTasks();

    assertEquals(0, getAppList()!.items!.length);
  });

  function getManagePermissionsInChromeRow(): CrLinkRowElement {
    const managePermissionsInChromeRow =
        privacyHubMicrophoneSubpage.shadowRoot!.querySelector<CrLinkRowElement>(
            '#managePermissionsInChromeRow');
    assertTrue(!!managePermissionsInChromeRow);
    return managePermissionsInChromeRow;
  }

  function getNoWebsiteHasAccessTextRow(): HTMLDivElement {
    const noWebsiteHasAccessTextRow =
        privacyHubMicrophoneSubpage.shadowRoot!.querySelector<HTMLDivElement>(
            '#noWebsiteHasAccessText');
    assertTrue(!!noWebsiteHasAccessTextRow);
    return noWebsiteHasAccessTextRow;
  }

  test('Websites section texts', async () => {
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('websitesSectionTitle'),
        privacyHubMicrophoneSubpage.shadowRoot!
            .querySelector('#websitesSectionTitle')!.textContent!.trim());

    assertEquals(
        privacyHubMicrophoneSubpage.i18n('manageMicPermissionsInChromeText'),
        getManagePermissionsInChromeRow().label);

    assertEquals(
        privacyHubMicrophoneSubpage.i18n('noWebsiteCanUseMicText'),
        getNoWebsiteHasAccessTextRow().textContent!.trim());
  });

  test('Websites section when microphone allowed', async () => {
    assertFalse(getManagePermissionsInChromeRow().hidden);
    assertTrue(getNoWebsiteHasAccessTextRow().hidden);
  });

  test('Websites section when microphone not allowed', async () => {
    mediaDevices.addDevice('audioinput', 'Fake Microphone');
    await waitAfterNextRender(privacyHubMicrophoneSubpage);

    // Toggle microphone access.
    getMicrophoneCrToggle().click();
    await waitAfterNextRender(privacyHubMicrophoneSubpage);

    assertTrue(getManagePermissionsInChromeRow().hidden);
    assertFalse(getNoWebsiteHasAccessTextRow().hidden);
  });

  test('Website section metric recorded when clicked', () => {
    assertEquals(
        0,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.WEBSITE_PERMISSION_LINK_CLICKED));

    getManagePermissionsInChromeRow().click();

    assertEquals(
        1,
        metrics.countMetricValue(
            'ChromeOS.PrivacyHub.MicrophoneSubpage.UserAction',
            PrivacyHubSensorSubpageUserAction.WEBSITE_PERMISSION_LINK_CLICKED));
  });

  test(
      'Clicking Chrome row opens Chrome browser microphone permission settings',
      async () => {
        assertEquals(
            PermissionType.kUnknown,
            fakeHandler.getLastOpenedBrowserPermissionSettingsType());

        getManagePermissionsInChromeRow()!.click();
        await fakeHandler.whenCalled('openBrowserPermissionSettings');

        assertEquals(
            PermissionType.kMicrophone,
            fakeHandler.getLastOpenedBrowserPermissionSettingsType());
      });

  test('System services section when microphone is allowed', async () => {
    assertEquals(
        privacyHubMicrophoneSubpage.i18n(
            'privacyHubSystemServicesSectionTitle'),
        privacyHubMicrophoneSubpage.shadowRoot!
            .querySelector('#systemServicesSectionTitle')!.textContent!.trim());

    await flushTasks();
    const systemServices =
        getSystemServicesFromSubpage(privacyHubMicrophoneSubpage);
    assertEquals(1, systemServices.length);
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('privacyHubSystemServicesAllowedText'),
        getSystemServicePermissionText(systemServices[0]!));
  });

  test('System services section when microphone is not allowed', async () => {
    mediaDevices.addDevice('audioinput', 'Fake Microphone');
    await flushTasks();
    // Toggle microphone access.
    getMicrophoneCrToggle().click();
    flush();

    const systemServices =
        getSystemServicesFromSubpage(privacyHubMicrophoneSubpage);
    assertEquals(1, systemServices.length);
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('privacyHubSystemServicesBlockedText'),
        getSystemServicePermissionText(systemServices[0]!));
  });
});
