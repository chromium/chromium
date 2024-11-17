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

  setup(() => {
    manager = new VoiceNotificationManager();
    notifications = {};
    listener = {
      notify(lang: string, type: NotificationType): void {
        notifications = {...notifications, [lang]: type};
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
});
