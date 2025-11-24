// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GatedSender} from 'chrome://glic/glic_api_impl/host/gated_sender.js';
import type {RequestMessage} from 'chrome://glic/glic_api_impl/post_message_transport.js';
import {PostMessageRequestSender} from 'chrome://glic/glic_api_impl/post_message_transport.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

class StubSender {
  sentMessages: RequestMessage[] = [];

  postMessage(message: any, _targetOrigin: string, _transfer?: Transferable[]):
      void {
    this.sentMessages.push(message);
  }
}

suite('GlicApiHost', () => {
  setup(() => {});

  function createSenders() {
    const stubSender = new StubSender();
    const gatedSender = new GatedSender(new PostMessageRequestSender(
        stubSender, 'origin', 'senderid', 'logPrefix'));
    return {stubSender, gatedSender};
  }

  test('GatedSender.sendWhenActive queues message', () => {
    const {stubSender, gatedSender} = createSenders();

    gatedSender.sendWhenActive('requestType' as any, {field: 'hi'});
    assertEquals(0, stubSender.sentMessages.length);

    gatedSender.setGating(false);
    assertEquals(1, stubSender.sentMessages.length);
    assertEquals('hi', stubSender.sentMessages[0]?.requestPayload.field);
  });

  test('GatedSender.sendWhenActive while ungated', () => {
    const {stubSender, gatedSender} = createSenders();
    gatedSender.setGating(false);
    gatedSender.sendWhenActive('requestType' as any, {field: 'hi'});
    assertEquals(1, stubSender.sentMessages.length);
    assertEquals('hi', stubSender.sentMessages[0]?.requestPayload.field);
  });

  test('GatedSender.sendIfActiveOrDrop while ungated', () => {
    const {stubSender, gatedSender} = createSenders();
    gatedSender.setGating(false);
    gatedSender.sendIfActiveOrDrop('requestType' as any, {field: 'hi'});

    assertEquals(1, stubSender.sentMessages.length);
    assertEquals('hi', stubSender.sentMessages[0]?.requestPayload.field);
  });

  test('GatedSender.sendIfActiveOrDrop while gated', () => {
    const {stubSender, gatedSender} = createSenders();
    gatedSender.sendIfActiveOrDrop('requestType' as any, {field: 'hi'});
    gatedSender.setGating(false);

    assertEquals(0, stubSender.sentMessages.length);
  });

  test('GatedSender.sendLatestWhenActive while gated', () => {
    const {stubSender, gatedSender} = createSenders();
    gatedSender.sendLatestWhenActive(
        'requestType' as any, {field: 'A'}, [], 'key');
    // Same request type, same key: should replace 'A'.
    gatedSender.sendLatestWhenActive(
        'requestType' as any, {field: 'B'}, [], 'key');
    // Different request type: should enqueue.
    gatedSender.sendLatestWhenActive(
        'requestType2' as any, {field: 'C'}, [], 'key');
    // Different request key: should enqueue.
    gatedSender.sendLatestWhenActive(
        'requestType2' as any, {field: 'D'}, [], 'key2');
    gatedSender.setGating(false);

    assertEquals(3, stubSender.sentMessages.length);
    assertEquals('B', stubSender.sentMessages[0]?.requestPayload.field);
    assertEquals('C', stubSender.sentMessages[1]?.requestPayload.field);
    assertEquals('D', stubSender.sentMessages[2]?.requestPayload.field);
  });

  test('GatedSender sends queued messages in order', () => {
    const {stubSender, gatedSender} = createSenders();
    gatedSender.sendLatestWhenActive(
        'requestType' as any, {field: 'A'}, [], 'key');
    gatedSender.sendLatestWhenActive(
        'requestType' as any, {field: 'B'}, [], 'key2');
    gatedSender.sendWhenActive('requestType' as any, {field: 'C'});
    gatedSender.sendLatestWhenActive(
        'requestType' as any, {field: 'D'}, [], 'key2');  // Replaces B.
    gatedSender.setGating(false);

    assertEquals(3, stubSender.sentMessages.length);
    assertEquals('A', stubSender.sentMessages[0]?.requestPayload.field);
    assertEquals('C', stubSender.sentMessages[1]?.requestPayload.field);
    assertEquals('D', stubSender.sentMessages[2]?.requestPayload.field);
  });

  test('GatedSender toggle gating doesnt send messages more than once', () => {
    const {stubSender, gatedSender} = createSenders();
    gatedSender.sendLatestWhenActive(
        'requestType' as any, {field: 'A'}, [], 'key');
    gatedSender.sendWhenActive('requestType' as any, {field: 'C'});
    gatedSender.setGating(false);
    gatedSender.setGating(true);
    gatedSender.setGating(false);

    assertEquals(2, stubSender.sentMessages.length);
  });
});
