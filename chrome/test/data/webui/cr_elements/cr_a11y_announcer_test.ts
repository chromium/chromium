// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';

import {getInstance, TIMEOUT_MS} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  test('QueuesMessages', async () => {
    const announcer = getInstance();
    const messagesDiv = announcer.shadowRoot!.querySelector('#messages')!;

    // Queue up 2 messages at once, and assert they both exist.
    const message1 = 'Knock knock!';
    const message2 = 'Who\'s there?';
    announcer.announce(message1);
    announcer.announce(message2);
    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    assertTrue(messagesDiv.textContent!.includes(message1));
    assertTrue(messagesDiv.textContent!.includes(message2));

    // Queue up 1 message, and assert it clears out previous messages.
    const message3 = 'No jokes allowed';
    announcer.announce(message3);
    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    assertFalse(messagesDiv.textContent!.includes(message1));
    assertFalse(messagesDiv.textContent!.includes(message2));
    assertTrue(messagesDiv.textContent!.includes(message3));
  });

  test('ClearsAnnouncerOnDisconnect', async () => {
    const announcer = getInstance();
    const lostMessage = 'You will never hear me.';
    announcer.announce(lostMessage);
    announcer.remove();
    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    assertFalse(
        announcer.shadowRoot!.querySelector('#messages')!.textContent!.includes(
            lostMessage));

    // Creates new announcer since previous announcer is removed from instances.
    assertNotEquals(announcer, getInstance());
  });

  test('SendsCustomEvent', async () => {
    const announcer = getInstance();
    const announcerEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    const message1 = 'Hello.';
    const message2 = 'Hi.';
    announcer.announce(message1);
    announcer.announce(message2);
    const announcerEvent = await announcerEventPromise;
    assertTrue(announcerEvent.detail.messages.includes(message1));
    assertTrue(announcerEvent.detail.messages.includes(message2));
  });
});
