// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {MediaDevicesProxy, PrivacyHubBrowserProxyImpl, SettingsPrivacyHubMicrophoneSubpage} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, CrToggleElement, PaperTooltipElement, Router} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMediaDevices} from '../fake_media_devices.js';

import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

suite('<settings-privacy-hub-microphone-subpage>', () => {
  let privacyHubMicrophoneSubpage: SettingsPrivacyHubMicrophoneSubpage;
  let privacyHubBrowserProxy: TestPrivacyHubBrowserProxy;
  let mediaDevices: FakeMediaDevices;

  setup(() => {
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

  function getNoMicrophoneText(): HTMLDivElement|null {
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

  test('Microphone section view when microphone access is enabled', () => {
    const microphoneToggle = getMicrophoneCrToggle();

    assertEquals(
        microphoneToggle.checked,
        privacyHubMicrophoneSubpage.prefs.ash.user.microphone_allowed.value);
    assertEquals(privacyHubMicrophoneSubpage.i18n('deviceOn'), getOnOffText());
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('microphoneToggleSubtext'),
        getOnOffSubtext());
    assertTrue(isMicrophoneListSectionVisible());
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
        assertEquals('Blocked for all', getOnOffSubtext());
        assertFalse(isMicrophoneListSectionVisible());
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
    }
  });

  test('No microphone connected by default', () => {
    assertNull(getMicrophoneList());
    assertTrue(!!getNoMicrophoneText());
    assertEquals(
        privacyHubMicrophoneSubpage.i18n('noMicrophoneConnectedText'),
        getNoMicrophoneText()!.textContent!.trim());
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
        assertNull(getNoMicrophoneText());
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
});
