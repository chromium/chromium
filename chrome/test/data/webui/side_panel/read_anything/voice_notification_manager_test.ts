// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {NotificationType, VoiceClientSideStatusCode, VoiceNotificationManager} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {VoiceNotificationListener} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse} from 'chrome-untrusted://webui-test/chai_assert.js';

suite('VoiceNotificationManager', () => {
  let manager: VoiceNotificationManager;
  let listener: VoiceNotificationListener;
  let notifications: {[lang: string]: NotificationType};
  let actualLang: string|undefined;
  let actualNotification: NotificationType;

  setup(() => {
    manager = new VoiceNotificationManager();
    notifications = {};
    listener = {
      notify(type: NotificationType, lang?: string): void {
        if (lang) {
          notifications = {...notifications, [lang]: type};
        }
        actualLang = lang;
        actualNotification = type;
      },
    };
  });

  test('new listeners are notified of in progress downloads', () => {
    manager.onVoiceStatusChange(
        'fr', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
    manager.onVoiceStatusChange(
        'yue', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
    manager.onVoiceStatusChange(
        'hi', VoiceClientSideStatusCode.ERROR_INSTALLING, []);
    manager.onVoiceStatusChange(
        'bn', VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION, []);

    manager.addListener(listener);

    assertEquals(NotificationType.DOWNLOADING, notifications['fr']);
    assertEquals(NotificationType.DOWNLOADING, notifications['yue']);
    assertFalse(!!notifications['hi']);
    assertFalse(!!notifications['bn']);
  });

  test('removed listeners are no longer notified', () => {
    manager.addListener(listener);
    manager.onVoiceStatusChange(
        'fr', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
    assertEquals(NotificationType.DOWNLOADING, notifications['fr']);

    manager.removeListener(listener);
    manager.onVoiceStatusChange(
        'yue', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
    manager.onVoiceStatusChange(
        'hi', VoiceClientSideStatusCode.ERROR_INSTALLING, []);
    assertFalse(!!notifications['yue']);
    assertFalse(!!notifications['hi']);
  });

  test('listeners are notified of all statuses', () => {
    manager.addListener(listener);

    manager.onVoiceStatusChange(
        'fr', VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
    manager.onVoiceStatusChange('yue', VoiceClientSideStatusCode.AVAILABLE, []);
    manager.onVoiceStatusChange(
        'hi', VoiceClientSideStatusCode.ERROR_INSTALLING, []);
    manager.onVoiceStatusChange(
        'bn', VoiceClientSideStatusCode.INSTALL_ERROR_ALLOCATION, []);

    assertEquals(NotificationType.DOWNLOADING, notifications['fr']);
    assertEquals(NotificationType.DOWNLOADED, notifications['yue']);
    assertEquals(NotificationType.GENERIC_ERROR, notifications['hi']);
    assertEquals(NotificationType.NO_SPACE, notifications['bn']);
  });

  test(
      'listener notified of Google Voices Unavailable without a language',
      () => {
        manager.addListener(listener);

        manager.onNoEngineConnection();

        assertEquals(
            actualNotification, NotificationType.GOOGLE_VOICES_UNAVAILABLE);
        assertEquals(actualLang, undefined);
      });

  test('listener notified of canceled download', () => {
    const lang = 'tr';
    manager.addListener(listener);

    manager.onVoiceStatusChange(
        lang, VoiceClientSideStatusCode.SENT_INSTALL_REQUEST, []);
    manager.onCancelDownload(lang);

    assertEquals(NotificationType.NONE, notifications[lang]);
  });

  test('listener not notified if canceled language is not downloading', () => {
    const lang = 'tr';
    manager.addListener(listener);

    manager.onCancelDownload(lang);

    assertFalse(!!notifications[lang]);
  });
});
