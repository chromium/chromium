// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {NotificationType, VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {BrowserProxy, mojoVoicePackStatusToVoicePackStatusEnum, VoiceClientSideStatusCode, VoiceNotificationManager, VoicePackController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';

suite('VoicePackController', () => {
  let voicePackController: VoicePackController;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    voicePackController = new VoicePackController();
  });

  suite('setLocalStatus', () => {
    let listener: VoiceNotificationListener;
    let listenerNotified: boolean;

    setup(() => {
      listenerNotified = false;
      listener = {
        notify(_type: NotificationType, _language: string): void {
          listenerNotified = true;
        },
      };
      VoiceNotificationManager.setInstance(new VoiceNotificationManager());
      VoiceNotificationManager.getInstance().addListener(listener);
    });

    test('no notification for non-Google language', () => {
      voicePackController.setLocalStatus(
          'zh', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for invalid language', () => {
      voicePackController.setLocalStatus(
          'klingon', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertFalse(listenerNotified);
    });

    test('no notification for same status', () => {
      voicePackController.setLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
      listenerNotified = false;

      voicePackController.setLocalStatus(
          'pt-br', VoiceClientSideStatusCode.ERROR_INSTALLING);

      assertFalse(listenerNotified);
    });

    test('notifies for new status with Google-supported language', () => {
      voicePackController.setLocalStatus(
          'it-it', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);

      listenerNotified = false;
      voicePackController.setLocalStatus(
          'it-it', VoiceClientSideStatusCode.AVAILABLE);
      assertTrue(listenerNotified);

      listenerNotified = false;
      voicePackController.setLocalStatus(
          'hi', VoiceClientSideStatusCode.ERROR_INSTALLING);
      assertTrue(listenerNotified);
    });
  });

  test('setServerStatus uses voice pack lang', () => {
    const status1 = mojoVoicePackStatusToVoicePackStatusEnum('kInstalled');
    const status2 = mojoVoicePackStatusToVoicePackStatusEnum('kOther');

    voicePackController.setServerStatus('de-de', status1);
    voicePackController.setServerStatus('yue-hk', status2);

    assertEquals(status1, voicePackController.getServerStatus('de-de'));
    assertEquals(status2, voicePackController.getServerStatus('yue-hk'));
    assertEquals(status1, voicePackController.getServerStatus('de'));
    assertEquals(status2, voicePackController.getServerStatus('yue'));
  });
});
