// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {defInterface, defMessage, GatedSender, ObservableValue, PostMessageRequestSender, PostMessageRouterImpl, Queue} from 'chrome://glic/glic.js';
import type {RequestMessage} from 'chrome://glic/glic.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// To trigger these tests, run tests in
// chrome/test/data/webui/glic/glic_browsertest.cc
class StubSender {
  sentMessages: RequestMessage[] = [];

  postMessage(message: any, _targetOrigin: string, _transfer?: Transferable[]):
      void {
    this.sentMessages.push(message);
  }
}

const DemoInterfaceDef = defInterface({
  name: 'DemoInterface',
  methods: [
    {
      name: 'requestType',
      request: defMessage<{field: string}>(),
      backgroundAllowed: true,
    },
    {
      name: 'requestType2',
      request: defMessage<{field: string}>(),
      backgroundAllowed: true,
    },
  ],
});
type DemoInterface = typeof DemoInterfaceDef;


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
  interface TestPayload {
    field: string;
  }

  function createSenders() {
    const stubSender = new StubSender();
    const router = new PostMessageRouterImpl(
        'origin', 'senderid', stubSender, 'logPrefix', true);
    new PostMessageRequestSender(router);
    const shouldGate = ObservableValue.withValue(true);
    const gatedSender = new GatedSender<DemoInterface>(
        router.newPipeWithRemote(DemoInterfaceDef).remote, shouldGate);
    return {stubSender, gatedSender, shouldGate};
  }

  test('GatedSender.sendWhenActive queues message', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();

    gatedSender.sendWhenActive('requestType', {field: 'hi'});
    assertEquals(0, stubSender.sentMessages.length);

    shouldGate.assignAndSignal(false);
    assertEquals(1, stubSender.sentMessages.length);
    assertEquals(
        'hi',
        (stubSender.sentMessages[0]?.requestPayload as TestPayload).field);
  });

  test('GatedSender.sendWhenActive while ungated', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();
    shouldGate.assignAndSignal(false);
    gatedSender.sendWhenActive('requestType', {field: 'hi'});
    assertEquals(1, stubSender.sentMessages.length);
    assertEquals(
        'hi',
        (stubSender.sentMessages[0]?.requestPayload as TestPayload).field);
  });

  test('GatedSender.sendIfActiveOrDrop while ungated', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();
    shouldGate.assignAndSignal(false);
    gatedSender.sendIfActiveOrDrop('requestType', {field: 'hi'});

    assertEquals(1, stubSender.sentMessages.length);
    assertEquals(
        'hi',
        (stubSender.sentMessages[0]?.requestPayload as TestPayload).field);
  });

  test('GatedSender.sendIfActiveOrDrop while gated', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();
    gatedSender.sendIfActiveOrDrop('requestType', {field: 'hi'});
    shouldGate.assignAndSignal(false);

    assertEquals(0, stubSender.sentMessages.length);
  });

  test('GatedSender.sendLatestWhenActive while gated', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();
    gatedSender.sendLatestWhenActive('requestType', {field: 'A'}, [], 'key');
    // Same request type, same key: should replace 'A'.
    gatedSender.sendLatestWhenActive('requestType', {field: 'B'}, [], 'key');
    // Different request type: should enqueue.
    gatedSender.sendLatestWhenActive('requestType2', {field: 'C'}, [], 'key');
    // Different request key: should enqueue.
    gatedSender.sendLatestWhenActive('requestType2', {field: 'D'}, [], 'key2');
    shouldGate.assignAndSignal(false);

    assertEquals(3, stubSender.sentMessages.length);
    assertEquals(
        'B', (stubSender.sentMessages[0]?.requestPayload as TestPayload).field);
    assertEquals(
        'C', (stubSender.sentMessages[1]?.requestPayload as TestPayload).field);
    assertEquals(
        'D', (stubSender.sentMessages[2]?.requestPayload as TestPayload).field);
  });

  test('GatedSender sends queued messages in order', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();
    gatedSender.sendLatestWhenActive('requestType', {field: 'A'}, [], 'key');
    gatedSender.sendLatestWhenActive('requestType', {field: 'B'}, [], 'key2');
    gatedSender.sendWhenActive('requestType', {field: 'C'});
    gatedSender.sendLatestWhenActive(
        'requestType', {field: 'D'}, [], 'key2');  // Replaces B.
    shouldGate.assignAndSignal(false);

    assertEquals(3, stubSender.sentMessages.length);
    assertEquals(
        'A', (stubSender.sentMessages[0]?.requestPayload as TestPayload).field);
    assertEquals(
        'C', (stubSender.sentMessages[1]?.requestPayload as TestPayload).field);
    assertEquals(
        'D', (stubSender.sentMessages[2]?.requestPayload as TestPayload).field);
  });

  test('GatedSender toggle gating doesnt send messages more than once', () => {
    const {stubSender, gatedSender, shouldGate} = createSenders();
    gatedSender.sendLatestWhenActive('requestType', {field: 'A'}, [], 'key');
    gatedSender.sendWhenActive('requestType', {field: 'C'});
    shouldGate.assignAndSignal(false);
    shouldGate.assignAndSignal(true);
    shouldGate.assignAndSignal(false);

    assertEquals(2, stubSender.sentMessages.length);
  });

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
