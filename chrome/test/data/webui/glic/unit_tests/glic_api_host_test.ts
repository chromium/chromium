// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageRequestSender, PostMessageRouterImpl, Queue} from '//webui-test/glic/glic_api_impl/transport/post_message_transport.js';
import type {RequestMessage} from '//webui-test/glic/glic_api_impl/transport/post_message_transport.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    assertTrue(q.empty());
    q.push(1);  // next=[1]
    assertFalse(q.empty());
    q.push(2);     // next=[1, 2]
    q.popFront();  // current=[∅, 2], next=[]
    assertFalse(q.empty());
    q.popFront();  // current=[∅, ∅]
    assertTrue(q.empty());
  });
});

suite('GlicApiHost', () => {

  test('PostMessageRequestSender limits in-flight requests', async () => {
    const stubSender = new StubSender();
    const router = new PostMessageRouterImpl(
        'origin', 'senderid', stubSender, 'logPrefix', true);
    const sender = new PostMessageRequestSender(router);
    sender.setMaxInFlightRequests(2);

    // Send 3 requests.
    const p1 = sender.requestWithResponse(0, 'type' as any, {});
    const p2 = sender.requestWithResponse(0, 'type' as any, {});
    const p3 = sender.requestWithResponse(0, 'type' as any, {});

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
        const router = new PostMessageRouterImpl(
            'origin', 'senderid', stubSender, 'logPrefix', true);
        const sender = new PostMessageRequestSender(router);
        sender.setMaxInFlightRequests(1);

        // Fill the in-flight slot.
        sender.requestWithResponse(0, 'type' as any, {});
        assertEquals(1, stubSender.sentMessages.length);

        // Call requestNoResponse.
        sender.requestNoResponse(0, 'typeNoRes' as any, {});

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
        assertTrue(stubSender.sentMessages[1]!.requestId !== undefined);
      });

  test('sendResponsesForAllRequests forces requestId on all messages', () => {
    const stubSender = new StubSender();
    const router = new PostMessageRouterImpl(
        'origin', 'senderid', stubSender, 'logPrefix', true);
    const sender = new PostMessageRequestSender(router);
    sender.sendResponsesForAllRequests = true;

    sender.requestNoResponse(0, 'typeNoRes' as any, {});

    assertEquals(1, stubSender.sentMessages.length);
    // Should have requestId even though it's "no response" because the mode is
    // on.
    assertTrue(stubSender.sentMessages[0]!.requestId !== undefined);
    assertEquals(1, sender.inFlightRequestCount());
  });
});
