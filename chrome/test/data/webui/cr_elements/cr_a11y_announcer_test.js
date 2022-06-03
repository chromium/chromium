// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrA11yAnnouncerElement, TIMEOUT_MS} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

suite('CrA11yAnnouncerElementTest', () => {
  setup(() => {
    document.body.innerHTML = '';
  });

  function getMessagesDiv(announcer) {
    return announcer.shadowRoot.querySelector('#messages');
  }

  test('CreatesAndGetsAnnouncers', () => {
    const defaultAnnouncer = CrA11yAnnouncerElement.getInstance();
    assertEquals(document.body, defaultAnnouncer.parentElement);
    assertEquals(defaultAnnouncer, CrA11yAnnouncerElement.getInstance());

    const dialog = document.createElement('dialog');
    document.body.appendChild(dialog);
    const dialogAnnouncer = CrA11yAnnouncerElement.getInstance(dialog);
    assertEquals(dialog, dialogAnnouncer.parentElement);
    assertEquals(dialogAnnouncer, CrA11yAnnouncerElement.getInstance(dialog));
  });

  test('QueuesMessages', async () => {
    const announcer = CrA11yAnnouncerElement.getInstance();
    const messagesDiv = announcer.shadowRoot.querySelector('#messages');

    // Queue up 2 messages at once, and assert they both exist.
    const message1 = 'Knock knock!';
    const message2 = 'Who\'s there?';
    announcer.announce(message1);
    announcer.announce(message2);
    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    assertTrue(messagesDiv.textContent.includes(message1));
    assertTrue(messagesDiv.textContent.includes(message2));

    // Queue up 1 message, and assert it clears out previous messages.
    const message3 = 'No jokes allowed';
    announcer.announce(message3);
    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    assertFalse(messagesDiv.textContent.includes(message1));
    assertFalse(messagesDiv.textContent.includes(message2));
    assertTrue(messagesDiv.textContent.includes(message3));
  });

  test('ClearsAnnouncerOnDisconnect', async () => {
    const announcer = CrA11yAnnouncerElement.getInstance();
    const lostMessage = 'You will never hear me.';
    announcer.announce(lostMessage);
    announcer.remove();
    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    assertFalse(announcer.shadowRoot.querySelector('#messages')
                    .textContent.includes(lostMessage));

    // Creates new announcer since previous announcer is removed from instances.
    assertNotEquals(announcer, CrA11yAnnouncerElement.getInstance());
  });
});
