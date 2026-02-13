// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GatedSender} from 'chrome://glic/glic_api_impl/host/gated_sender.js';
import type {RequestMessage} from 'chrome://glic/glic_api_impl/post_message_transport.js';
import {PostMessageRequestSender, PostMessageRouter, Queue} from 'chrome://glic/glic_api_impl/post_message_transport.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

// To trigger these tests, run tests in
// chrome/test/data/webui/glic/glic_browsertest.cc
class StubSender {
  sentMessages: RequestMessage[] = [];

  postMessage(message: any, _targetOrigin: string, _transfer?: Transferable[]):
      void {
    this.sentMessages.push(message);
  }
}
suite('Queue', () => {
  test('Push and popFront in order', () => {
    const q = new Queue<number>();
    q.push(1);
    q.push(2);
    assertEquals(1, q.popFront());
    assertEquals(2, q.popFront());
    assertEquals(undefined, q.popFront());
  });

  test('Correct length reporting', () => {
    const q = new Queue<number>();
    assertEquals(0, q.length);
    q.push(1);
    q.push(2);
    assertEquals(2, q.length);
    q.popFront();
    assertEquals(1, q.length);
    q.push(3);
    assertEquals(2, q.length);
    q.popFront();
    assertEquals(1, q.length);
    q.popFront();
    assertEquals(0, q.length);
  });

  test('Resets index when next becomes current', () => {
    const q = new Queue<number>();
    q.push(1);
    q.push(2);
    assertEquals(1, q.popFront());  // current=[∅, 2]
    q.push(3);                      // next=[3]
    assertEquals(2, q.popFront());  // current=[∅, ∅]
    assertEquals(3, q.popFront());  // current=[∅]
    assertEquals(undefined, q.popFront());
  });

  test('empty() checks accurately', () => {
    const q = new Queue<number>();
    assertEquals(true, q.empty());
    q.push(1);  // next=[1]
    assertEquals(false, q.empty());
    q.push(2);     // next=[1, 2]
    q.popFront();  // current=[∅, 2], next=[]
    assertEquals(false, q.empty());
    q.popFront();  // current=[∅, ∅]
    assertEquals(true, q.empty());
  });
});

suite('GlicApiHost', () => {
  setup(() => {});

  function createSenders() {
    const stubSender = new StubSender();
    const router =
        new PostMessageRouter('origin', 'senderid', stubSender, 'logPrefix');
    const gatedSender = new GatedSender(new PostMessageRequestSender(router));
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

  test('PostMessageRequestSender limits in-flight requests', async () => {
    const stubSender = new StubSender();
    const router =
        new PostMessageRouter('origin', 'senderid', stubSender, 'logPrefix');
    const sender = new PostMessageRequestSender(router);
    sender.setMaxInFlightRequests(2);

    // Send 3 requests.
    const p1 = sender.requestWithResponse('type' as any, {});
    const p2 = sender.requestWithResponse('type' as any, {});
    const p3 = sender.requestWithResponse('type' as any, {});

    // Only 2 should be sent immediately.
    assertEquals(2, stubSender.sentMessages.length);
    assertEquals(2, sender.inFlightRequestCount());
    assertEquals(1, sender.messageQueueLength());

    // Resolve one request.
    router.onMessage({
      data: {
        responseId: stubSender.sentMessages[0]!.requestId,
        senderId: 'senderid',  // Must match for responses.
        type: 'type',
        responsePayload: 'res1',
      },
      origin: 'origin',
      source: {} as any,
    } as any);

    const res1 = await p1;
    assertEquals('res1', res1);

    // Now the 3rd request should have been sent.
    assertEquals(3, stubSender.sentMessages.length);
    assertEquals(2, sender.inFlightRequestCount());
    assertEquals(0, sender.messageQueueLength());

    // Resolve the rest.
    router.onMessage({
      data: {
        responseId: stubSender.sentMessages[1]!.requestId,
        senderId: 'senderid',
        type: 'type',
        responsePayload: 'res2',
      },
      origin: 'origin',
      source: {} as any,
    } as any);
    router.onMessage({
      data: {
        responseId: stubSender.sentMessages[2]!.requestId,
        senderId: 'senderid',
        type: 'type',
        responsePayload: 'res3',
      },
      origin: 'origin',
      source: {} as any,
    } as any);

    assertEquals('res2', await p2);
    assertEquals('res3', await p3);
    assertEquals(0, sender.inFlightRequestCount());
  });

  test(
      'requestNoResponse is upgraded to requestWithResponse when queueing',
      () => {
        const stubSender = new StubSender();
        const router = new PostMessageRouter(
            'origin', 'senderid', stubSender, 'logPrefix');
        const sender = new PostMessageRequestSender(router);
        sender.setMaxInFlightRequests(1);

        // Fill the in-flight slot.
        sender.requestWithResponse('type' as any, {});
        assertEquals(1, stubSender.sentMessages.length);

        // Call requestNoResponse.
        sender.requestNoResponse('typeNoRes' as any, {});

        // It should be queued, so not sent yet.
        assertEquals(1, stubSender.sentMessages.length);
        assertEquals(1, sender.messageQueueLength());

        // Fulfilling the first request should trigger sending the second one.
        // Importantly, it should have a requestId now.
        router.onMessage({
          data: {
            responseId: stubSender.sentMessages[0]!.requestId,
            senderId: 'senderid',
            type: 'type',
            responsePayload: {},
          },
          origin: 'origin',
          source: {} as any,
        } as any);

        assertEquals(2, stubSender.sentMessages.length);
        assertEquals('typeNoRes', stubSender.sentMessages[1]!.type);
        // It was upgraded to have a requestId.
        assertEquals(true, stubSender.sentMessages[1]!.requestId !== undefined);
      });

  test('sendResponsesForAllRequests forces requestId on all messages', () => {
    const stubSender = new StubSender();
    const router =
        new PostMessageRouter('origin', 'senderid', stubSender, 'logPrefix');
    const sender = new PostMessageRequestSender(router);
    sender.sendResponsesForAllRequests = true;

    sender.requestNoResponse('typeNoRes' as any, {});

    assertEquals(1, stubSender.sentMessages.length);
    // Should have requestId even though it's "no response" because the mode is
    // on.
    assertEquals(true, stubSender.sentMessages[0]!.requestId !== undefined);
    assertEquals(1, sender.inFlightRequestCount());
  });
});
