// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {createBidirectionalPostMessageTransport, defInterface, defMessage, InverseSet, ON_PIPE_CLOSED} from 'chrome://glic/glic.js';
import type {ErrorCodec, InterfaceDef, PendingReceiver, PendingRemote, PostMessageHandler, PostMessageLifecycleObserver, PostMessageRouterImpl, PostMessageSender, TransferableException} from 'chrome://glic/glic.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertRejects, sleep, waitUntilEqual} from './test_helpers.js';

const TEST_ERROR_CODEC: ErrorCodec = {
  serialize(e: Error): TransferableException {
    return {exception: e};
  },
  deserialize(raw: TransferableException): Error {
    return raw.exception;
  },
};

class FakePostMessageSender implements PostMessageSender {
  router?: PostMessageRouterImpl;
  sendOrigin: string = '';

  postMessage(
      message: unknown, _targetOrigin: string,
      _transfer?: Transferable[]): void {
    setTimeout(() => {
      if (this.router) {
        this.router.onMessage({
          data: message,
          origin: this.sendOrigin,
          source: {} as unknown as MessageEventSource,
        } as MessageEvent);
      }
    }, 0);
  }
}

const CandyApiDef = defInterface({
  name: 'CandyApi',
  methods: [
    {
      name: 'lick',
      request: defMessage<{times: number}>(),
      response: defMessage<{result: string}>(),
    },
  ],
});
type CandyApi = typeof CandyApiDef;

const TestClientApiDef = defInterface({
  name: 'TestClientApi',
  methods: [
    {
      name: 'ping',
      request: defMessage<{msg: string}>(),
      response: defMessage<{reply: string}>(),
    },
    {
      name: 'throwError',
      request: defMessage<{msg: string}>(),
    },
    {
      name: 'newCandy',
      request: defMessage<{name: string}>(),
      response: defMessage<{remote: PendingRemote<CandyApi>}>(),
    },
    {
      name: 'newPendingCandy',
      request:
          defMessage<{name: string, receiver: PendingReceiver<CandyApi>}>(),
    },
    {
      name: 'lickTheCandy',
      request: defMessage<{remote: PendingRemote<CandyApi>}>(),
    },
  ],
});
type TestClientApi = typeof TestClientApiDef;

const TestHostApiDef = defInterface({
  name: 'TestHostApi',
  methods: [
    {
      name: 'notify',
      request: defMessage<{info: string}>(),
    },
  ],
});
type TestHostApi = typeof TestHostApiDef;

class TrackingLifecycleObserver implements PostMessageLifecycleObserver {
  calls: Array<{event: 'received' | 'exception' | 'completed', type: string}> =
      [];

  onRequestReceived(type: string, _interfaceDef: InterfaceDef|undefined): void {
    this.calls.push({event: 'received', type});
  }

  onRequestHandlerException(
      type: string, _interfaceDef: InterfaceDef|undefined): void {
    this.calls.push({event: 'exception', type});
  }

  onRequestCompleted(type: string, _interfaceDef: InterfaceDef|undefined):
      void {
    this.calls.push({event: 'completed', type});
  }
}

class CandyHandler implements PostMessageHandler<CandyApi> {
  lickCount = 0;
  isClosed = false;
  constructor(public name?: string) {}
  lick(payload: {times: number}): {result: string} {
    this.lickCount += payload.times;
    return {result: `licked ${this.name} ${this.lickCount} times`};
  }

  [ON_PIPE_CLOSED](): void {
    this.isClosed = true;
  }
}

class ClientHandler implements PostMessageHandler<TestClientApi> {
  constructor(public router?: PostMessageRouterImpl) {}
  calls: Array<{type: string, payload: unknown}> = [];
  candies: CandyHandler[] = [];

  ping(payload: {msg: string}): {reply: string} {
    this.calls.push({type: 'ping', payload});
    return {reply: `pong: ${payload.msg}`};
  }
  throwError(payload: {msg: string}): void {
    this.calls.push({type: 'throwError', payload});
    throw new Error(payload.msg);
  }

  newCandy(payload: {name: string}): {remote: PendingRemote<CandyApi>} {
    assert(this.router);
    const handler = new CandyHandler(payload.name);
    const {remote} = this.router.newPipeWithReceiver(handler, CandyApiDef);
    this.candies.push(handler);
    return {remote};
  }
  newPendingCandy(payload: {name: string, receiver: PendingReceiver<CandyApi>}):
      void {
    assert(this.router);
    const handler = new CandyHandler(payload.name);
    this.candies.push(handler);
    this.router.newReceiver(payload.receiver, handler, CandyApiDef);
  }
  async lickTheCandy(payload: {remote: PendingRemote<CandyApi>}):
      Promise<void> {
    assert(this.router);
    const remote = this.router.newRemote(payload.remote);
    await remote.requestWithResponse('lick', {times: 1});
  }
}

class HostHandler implements PostMessageHandler<TestHostApi> {
  calls: Array<{type: string, payload: unknown}> = [];

  notify(payload: {info: string}): void {
    this.calls.push({type: 'notify', payload});
  }
}

suite('PostMessageTransportTest', () => {
  let hostTarget: FakePostMessageSender;
  let clientTarget: FakePostMessageSender;
  let routers: PostMessageRouterImpl[] = [];

  let hostHandler: HostHandler;
  let clientHandler: ClientHandler;

  setup(() => {
    hostTarget = new FakePostMessageSender();
    clientTarget = new FakePostMessageSender();
    hostHandler = new HostHandler();
    clientHandler = new ClientHandler();
    routers = [];
  });

  teardown(() => {
    for (const r of routers) {
      r.destroy();
    }
  });

  function connect() {
    const hostObserver = new TrackingLifecycleObserver();
    const clientObserver = new TrackingLifecycleObserver();

    const host = createBidirectionalPostMessageTransport(
        'client-origin', hostTarget, hostObserver, hostHandler, 'host', true,
        TEST_ERROR_CODEC, TestHostApiDef, TestClientApiDef);

    const client = createBidirectionalPostMessageTransport(
        'host-origin', clientTarget, clientObserver, clientHandler, 'client',
        false, TEST_ERROR_CODEC, TestClientApiDef, TestHostApiDef);

    hostTarget.router = client.router;
    clientTarget.router = host.router;
    clientHandler.router = client.router;

    hostTarget.sendOrigin = 'host-origin';
    clientTarget.sendOrigin = 'client-origin';

    routers.push(host.router);
    routers.push(client.router);

    return {host, client, hostObserver, clientObserver};
  }

  test('Successful bidirectional ping-pong request and response', async () => {
    const {host} = connect();
    const result =
        await host.rootRemote.requestWithResponse('ping', {msg: 'hello'});
    assertEquals('pong: hello', result.reply);
    assertEquals(1, clientHandler.calls.length);
    assertDeepEquals({msg: 'hello'}, clientHandler.calls[0]!.payload);
  });

  test('Successful request with no response expected', async () => {
    const {client} = connect();
    client.rootRemote.requestNoResponse('notify', {info: 'ready'});
    // Allow async dispatch to run
    await waitUntilEqual(() => hostHandler.calls.length, 1);
    assertDeepEquals({info: 'ready'}, hostHandler.calls[0]!.payload);
  });

  test('Pipe isolated from other pipes', async () => {
    const {host} = connect();
    // Create a new pipe and send a message. It should not be received by
    // the remote bound to the original pipe.
    const {remote} = host.router.newPipeWithRemote(TestClientApiDef);
    remote.requestWithResponse('ping', {msg: 'wrong pipe'});
    const result =
        await host.rootRemote.requestWithResponse('ping', {msg: 'hello'});
    assertEquals('pong: hello', result.reply);
    assertEquals(1, clientHandler.calls.length);
    assertDeepEquals({msg: 'hello'}, clientHandler.calls[0]!.payload);
  });

  test('Using a pending receiver', async () => {
    const {host} = connect();
    // Create a new pipe and send a message. It should not be received by
    // the remote bound to the original pipe.
    const {remote, receiver} = host.router.newPipeWithRemote(CandyApiDef);
    host.rootRemote.requestNoResponse(
        'newPendingCandy', {name: 'chocolate', receiver});
    const result = await remote.requestWithResponse('lick', {times: 1});
    assertEquals('licked chocolate 1 times', result.result);
  });

  test('Using a pending remote in response', async () => {
    const {host} = connect();
    const {remote: pendingRemote} =
        await host.rootRemote.requestWithResponse('newCandy', {name: 'mint'});
    const remote = host.router.newRemote(pendingRemote);
    const result = await remote.requestWithResponse('lick', {times: 2});
    assertEquals('licked mint 2 times', result.result);
  });

  test('Using a pending remote in request', async () => {
    const {host} = connect();
    const candyHandler = new CandyHandler('jawbreaker');
    const {remote} = host.router.newPipeWithReceiver(candyHandler, CandyApiDef);
    await host.rootRemote.requestWithResponse('lickTheCandy', {remote});
    assertEquals(1, candyHandler.lickCount);
  });

  test('Multiple pipes with pending remotes in response', async () => {
    const {host} = connect();
    const candyHandler1 = new CandyHandler('sucker');
    const candyHandler2 = new CandyHandler('jellybean');
    const {remote: remote1, receiver: receiver1} =
        host.router.newPipeWithReceiver(candyHandler1, CandyApiDef);
    const {remote: remote2} =
        host.router.newPipeWithReceiver(candyHandler2, CandyApiDef);
    await host.rootRemote.requestWithResponse(
        'lickTheCandy', {remote: remote1});
    assertEquals(1, candyHandler1.lickCount);
    assertEquals(0, candyHandler2.lickCount);

    // Close the first receiver. The second receiver should still work.
    receiver1.close();
    await host.rootRemote.requestWithResponse(
        'lickTheCandy', {remote: remote2});
    assertEquals(1, candyHandler2.lickCount);
  });

  test('Binding a receiver with pending messages', async () => {
    const {host} = connect();
    const candyHandler = new CandyHandler('jawbreaker');
    const {remote, receiver: pendingReceiver} = host.router.newPipe<CandyApi>();
    const lickPromise =
        host.rootRemote.requestWithResponse('lickTheCandy', {remote});
    await sleep(1);
    // The client will have tried to send on the pipe, but it wasn't bound yet.
    host.router.newReceiver(pendingReceiver, candyHandler, CandyApiDef);
    assertEquals(0, candyHandler.lickCount);
    await lickPromise;
    assertEquals(1, candyHandler.lickCount);
  });

  test('Closing a receiver with pending messages', async () => {
    const {host} = connect();
    const {remote, receiver: pendingReceiver} = host.router.newPipe<CandyApi>();
    const lickPromise =
        host.rootRemote.requestWithResponse('lickTheCandy', {remote});
    await sleep(1);
    const candyHandler = new CandyHandler();
    const receiver =
        host.router.newReceiver(pendingReceiver, candyHandler, CandyApiDef);
    receiver.close();
    await assertRejects(lickPromise, {withErrorMessage: 'Pipe closed'});
  });

  test('Closing a receiver', async () => {
    const {host} = connect();
    const candyHandler = new CandyHandler('gum');
    const {receiver, remote} =
        host.router.newPipeWithReceiver(candyHandler, CandyApiDef);
    receiver.close();
    assertTrue(candyHandler.isClosed);
    await assertRejects(
        host.rootRemote.requestWithResponse('lickTheCandy', {remote}),
        {withErrorMessage: 'Pipe closed'});
  });

  test('Closing a bound remote', async () => {
    const {host} = connect();
    const {receiver, remote} = host.router.newPipeWithRemote(CandyApiDef);
    await host.rootRemote.requestWithResponse(
        'newPendingCandy', {name: 'mint', receiver});
    remote.close();

    // Sending on a closed remote triggers an exception.
    await assertRejects(
        remote.requestWithResponse('lick', {times: 1}),
        {withErrorMessage: 'Pipe closed'});

    // The close handler should be called on the remote end as well.
    await waitUntilEqual(() => clientHandler.candies[0]!.isClosed, true);
  });

  test('Binding to a closed remote', async () => {
    const {host} = connect();
    const {receiver, remote} = host.router.newPipeWithRemote(CandyApiDef);
    remote.close();
    // Send the pending receiver, when the client tries to use it, it will be
    // closed.
    await host.rootRemote.requestWithResponse(
        'newPendingCandy', {name: 'taffy', receiver});
    assertTrue(!!clientHandler.candies[0]);
    assertTrue(clientHandler.candies[0].isClosed);
  });

  test('Error propagation across transport', async () => {
    const {host} = connect();
    await assertRejects(
        host.rootRemote.requestWithResponse(
            'throwError', {msg: 'fatal failure'}),
        {withErrorMessage: 'fatal failure'});
  });

  test('Pipe close triggers close handlers', async () => {
    const {host, client} = connect();

    let hostClosed = false;
    let clientClosed = false;

    host.rootRemote.addCloseHandler(() => {
      hostClosed = true;
    });

    client.rootReceiver.addCloseHandler(() => {
      clientClosed = true;
    });

    // Close the remote on host side
    host.rootRemote.close();

    await waitUntilEqual(() => hostClosed, true);
    await waitUntilEqual(() => clientClosed, true);
  });

  test('LifecycleObserver callbacks are called', async () => {
    const {host, clientObserver} = connect();

    // 1. Successful request: should trigger onRequestReceived and
    // onRequestCompleted
    await host.rootRemote.requestWithResponse('ping', {msg: 'hello'});
    assertEquals(clientObserver.calls.length, 2);
    assertDeepEquals(
        [
          {event: 'received', type: 'ping'},
          {event: 'completed', type: 'ping'},
        ],
        clientObserver.calls);

    // 2. Request throwing exception: should trigger onRequestReceived and
    // onRequestHandlerException
    clientObserver.calls = [];
    await assertRejects(
        host.rootRemote.requestWithResponse('throwError', {msg: 'error'}),
        {withErrorMessage: 'error'});
    assertEquals(clientObserver.calls.length, 2);
    assertDeepEquals(
        [
          {event: 'received', type: 'throwError'},
          {event: 'exception', type: 'throwError'},
        ],
        clientObserver.calls);
  });

  test('InverseSet', () => {
    const set = new InverseSet();
    const internal =
        (set as unknown as {notContained: Set<number>, maxValue: number});
    assertFalse(set.has(0));
    assertFalse(set.has(100));
    assertEquals(internal.notContained.size, 0);

    // Does not grow on delete.
    set.delete(100);
    assertEquals(internal.notContained.size, 0);
    assertFalse(set.has(100));

    // Grows on add.
    set.add(2);
    assertFalse(set.has(0));
    assertFalse(set.has(1));
    assertTrue(set.has(2));
    assertFalse(set.has(3));
    assertEquals(internal.notContained.size, 2);  // contains [0, 1]

    // Remove item.
    set.delete(2);
    assertFalse(set.has(2));

    // Add item within max value.
    set.add(1);
    assertTrue(set.has(1));
  });
});
