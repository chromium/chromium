// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';

import type {CrA11yAnnouncerElement} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {getInstance, TIMEOUT_MS} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

function waitForTimeout(): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
}

suite('CrA11yAnnouncerElementTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('CreatesAndGetsAnnouncers', () => {
    const defaultAnnouncer = getInstance();
    assertEquals(document.body, defaultAnnouncer.parentElement);
    assertEquals(defaultAnnouncer, getInstance());

    const dialog = document.createElement('dialog');
    document.body.appendChild(dialog);
    const dialogAnnouncer = getInstance(dialog);
    assertEquals(dialog, dialogAnnouncer.parentElement);
    assertEquals(dialogAnnouncer, getInstance(dialog));
  });

  suite('AriaNotify', () => {
    let announcer: CrA11yAnnouncerElement;
    let notifications: string[];

    setup(() => {
      announcer = getInstance();
      notifications = [];
      // Stub ariaNotify() on the element it is expected to be called on, so
      // that the test does not depend on whether the native API is available
      // on the current platform.
      announcer.$.messages.ariaNotify = (message: string) => {
        notifications.push(message);
      };
    });

    test('NotifiesQueuedMessages', async () => {
      const message1 = 'Knock knock!';
      const message2 = 'Who\'s there?';
      announcer.announce(message1);
      announcer.announce(message2);
      assertDeepEquals([], notifications);
      await waitForTimeout();
      assertDeepEquals([message1, message2], notifications);

      const message3 = 'No jokes allowed';
      announcer.announce(message3);
      await waitForTimeout();
      assertDeepEquals([message1, message2, message3], notifications);

      // The live region is not used when ariaNotify() is available, to avoid
      // announcing the messages twice.
      assertEquals('', announcer.$.messages.textContent.trim());
    });

    test('NotifiesConsecutiveIdenticalMessages', async () => {
      const message = 'Copied to clipboard';
      announcer.announce(message);
      await waitForTimeout();
      announcer.announce(message);
      await waitForTimeout();
      assertDeepEquals([message, message], notifications);
    });

    test('SendsCustomEvent', async () => {
      const announcerEventPromise =
          eventToPromise('cr-a11y-announcer-messages-sent', document.body);
      const message1 = 'Hello.';
      const message2 = 'Hi.';
      announcer.announce(message1);
      announcer.announce(message2);
      const announcerEvent = await announcerEventPromise;
      assertDeepEquals([message1, message2], announcerEvent.detail.messages);
      assertDeepEquals([message1, message2], notifications);
    });

    test('ClearsAnnouncerOnDisconnect', async () => {
      announcer.announce('You will never hear me.');
      announcer.remove();
      await waitForTimeout();
      assertDeepEquals([], notifications);

      // Creates new announcer since previous announcer is removed from
      // instances.
      assertNotEquals(announcer, getInstance());
    });
  });

  suite('LiveRegionFallback', () => {
    let announcer: CrA11yAnnouncerElement;
    let messagesDiv: HTMLElement;

    setup(() => {
      announcer = getInstance();
      messagesDiv = announcer.$.messages;
      // Simulate platforms where ariaNotify() is unavailable, e.g. ChromeOS.
      Object.defineProperty(
          messagesDiv, 'ariaNotify', {value: undefined, configurable: true});
    });

    test('QueuesMessages', async () => {
      // Queue up 2 messages at once, and assert they both exist.
      const message1 = 'Knock knock!';
      const message2 = 'Who\'s there?';
      announcer.announce(message1);
      announcer.announce(message2);
      await waitForTimeout();
      assertTrue(messagesDiv.textContent.includes(message1));
      assertTrue(messagesDiv.textContent.includes(message2));

      // Queue up 1 message, and assert it clears out previous messages.
      const message3 = 'No jokes allowed';
      announcer.announce(message3);
      await waitForTimeout();
      assertFalse(messagesDiv.textContent.includes(message1));
      assertFalse(messagesDiv.textContent.includes(message2));
      assertTrue(messagesDiv.textContent.includes(message3));
    });

    test('SendsCustomEvent', async () => {
      const announcerEventPromise =
          eventToPromise('cr-a11y-announcer-messages-sent', document.body);
      const message1 = 'Hello.';
      const message2 = 'Hi.';
      announcer.announce(message1);
      announcer.announce(message2);
      const announcerEvent = await announcerEventPromise;
      assertDeepEquals([message1, message2], announcerEvent.detail.messages);
      assertTrue(messagesDiv.textContent.includes(message1));
      assertTrue(messagesDiv.textContent.includes(message2));
    });

    test('ClearsAnnouncerOnDisconnect', async () => {
      const lostMessage = 'You will never hear me.';
      announcer.announce(lostMessage);
      announcer.remove();
      await waitForTimeout();
      assertFalse(messagesDiv.textContent.includes(lostMessage));

      // Creates new announcer since previous announcer is removed from
      // instances.
      assertNotEquals(announcer, getInstance());
    });
  });
});
