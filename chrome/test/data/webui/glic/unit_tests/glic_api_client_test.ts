// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ObservableSetByTabId} from '//webui-test/glic/glic_api_impl/client/observable_set_by_tab_id.js';
import type {ObservableSetByTabIdDelegate} from '//webui-test/glic/glic_api_impl/client/observable_set_by_tab_id.js';
import {WebClientHostDef} from '//webui-test/glic/glic_api_impl/request_types.js';
import type {WebClientHost} from '//webui-test/glic/glic_api_impl/request_types.js';
import {defInterface} from '//webui-test/glic/glic_api_impl/transport/messaging.js';
import type {InterfaceDef} from '//webui-test/glic/glic_api_impl/transport/messaging.js';
import {createDirectMessagingPair} from '//webui-test/glic/glic_api_impl/transport/post_message_transport.js';
import type {PendingRemote, PostMessageRemote, PostMessageRouter} from '//webui-test/glic/glic_api_impl/transport/post_message_transport.js';
import type {ObservableValue} from '//webui-test/glic/observable.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

interface TestEnvironment {
  sender: PostMessageRemote<WebClientHost>;
  delegate: TestDelegate;
  obs: ObservableSetByTabId<string>;
}

interface CurrentSubscription {
  tabId: string;
}

const DummyInterfaceDef = defInterface({
  name: 'DummyInterface',
  methods: [],
});

class TestDelegate implements
    ObservableSetByTabIdDelegate<string, InterfaceDef> {
  readonly interfaceDef = DummyInterfaceDef;
  readonly unsubscribeDelay = 10;
  observations: CurrentSubscription[] = [];

  constructor(private router: PostMessageRouter) {}

  subscribe(
      _sender: PostMessageRemote<WebClientHost>, tabId: string,
      remote: PendingRemote<InterfaceDef>): void {
    // Wrap tabId in a new object literal to create a unique reference.
    const sub = {tabId};
    this.observations.push(sub);
    this.router.addCloseHandler(remote, () => {
      this.observations = this.observations.filter(s => s !== sub);
    });
  }

  createHandler(obs: ObservableValue<string>): Record<never, never> {
    return new TestHandler(obs) as any;
  }
}

class TestHandler {
  constructor(private obs: ObservableValue<string>) {}
  onUpdate(value?: string) {
    if (value === undefined) {
      this.obs.complete();
    } else {
      this.obs.assignAndSignal(value);
    }
  }
}

function sleep(timeout: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, timeout));
}

suite('ObservableSetByTabId', () => {
  function createEnvironment(): TestEnvironment {
    const pair = createDirectMessagingPair<WebClientHost, InterfaceDef>(
        'test',
        /*errorCodec=*/ {} as any,
        /*clientRootHandler=*/ {} as any,
        /*hostRootHandler=*/ {} as any,
        /*clientInterfaceDef=*/ DummyInterfaceDef,
        /*hostInterfaceDef=*/ WebClientHostDef,
    );
    const router = pair.client.router;
    const sender =
        router.newPipeWithRemote<WebClientHost>(WebClientHostDef).remote;
    const delegate = new TestDelegate(router);
    const obs = new ObservableSetByTabId<string>(delegate, sender, router);
    return {sender, delegate, obs};
  }

  test('send with no observers', () => {
    const env = createEnvironment();
    // Does nothing.
    env.obs.getObservableByTabId('4').assignAndSignal('HI');
  });

  test('subscribe to tab id', () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');
    assertEquals(
        obs.getCurrentValue(), undefined, 'Initial value is incorrect');
    obs.subscribe((value) => {
      assertEquals(value, 'HI', 'Notified value is incorrect');
    });
    assertEquals(env.delegate.observations.length, 1);
    assertEquals(env.delegate.observations[0]!.tabId, '123');
    obs.assignAndSignal('HI');
    assertEquals(obs.getCurrentValue(), 'HI', 'getCurrentValue() is incorrect');
  });

  test('completeObservable removes subscription', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');
    assertEquals(
        obs.getCurrentValue(), undefined, 'Initial value is incorrect');
    let completed = false;
    obs.subscribe({
      complete() {
        completed = true;
      },
      next() {},
    });
    assertEquals(
        env.delegate.observations.length, 1, 'Subscription was not created');
    assertEquals(env.delegate.observations[0]!.tabId, '123');

    obs.complete();
    assertTrue(completed, 'complete() was not called');
    // wait for prune
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 0, 'Subscription was not removed');
  });

  test('subscribe after unsubscribe before prune', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    const sub1 = obs.subscribe(() => {});
    assertEquals(
        env.delegate.observations.length, 1, 'observation was not created');

    sub1.unsubscribe();

    // Subscribe before the original subscription is pruned. This should reuse
    // the first observation.
    const sub2 = obs.subscribe(() => {});

    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 1,
        'just one observation after second subscribe');

    sub2.unsubscribe();

    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 0, 'observation should be removed');
  });

  test('subscribe after prune', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    const sub1 = obs.subscribe(() => {});
    assertEquals(
        env.delegate.observations.length, 1,
        'first observation was not created');

    sub1.unsubscribe();
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 0,
        'first observation should be removed');

    // Subscribe after the original subscription is pruned. This should create
    // a new observation.
    const sub2 = env.obs.getObservableByTabId('123').subscribe(() => {});
    assertEquals(
        env.delegate.observations.length, 1,
        'second observation was not created');

    sub2.unsubscribe();
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 0,
        'second observation should be removed');
  });

  test('multiple concurrent subscribers (deduplication)', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    // First subscriber
    const sub1 = obs.subscribe(() => {});
    assertEquals(
        env.delegate.observations.length, 1,
        'First sub should trigger delegate');

    // Second subscriber
    assertEquals(
        obs, env.obs.getObservableByTabId('123'), 'Should get same observer');
    const sub2 = obs.subscribe(() => {});
    assertEquals(
        env.delegate.observations.length, 1,
        'Second sub should not trigger duplicate delegate call');
    // First unsubscribes
    sub1.unsubscribe();
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 1,
        'Delegate observation should remain active while second sub exists');

    // Second unsubscribes
    sub2.unsubscribe();
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 0,
        'Delegate observation should be removed after last sub unsubscribes');
  });

  test(
      'requesting a tab after completeObservable yields a new observable',
      async () => {
        const env = createEnvironment();
        const obs1 = env.obs.getObservableByTabId('foo');
        // Subscribe to force observation generation
        const sub1 = obs1.subscribe(() => {});

        // Calling complete queues a prune operation.
        obs1.complete();

        await sleep(env.delegate.unsubscribeDelay + 1);
        await sleep(0);

        // After prune, requesting the same tab should return a fresh observable
        // instance
        const obs2 = env.obs.getObservableByTabId('foo');
        if (obs1 === obs2) {
          throw new Error(
              'Expected new observable instance, but got the same one');
        }
        sub1.unsubscribe();
      });

  test('two different tabs can be observed independently', async () => {
    const env = createEnvironment();
    const obsA = env.obs.getObservableByTabId('tabA');
    const obsB = env.obs.getObservableByTabId('tabB');
    assertNotEquals(obsA, obsB, 'Should get different observers');
    const subA = obsA.subscribe(() => {});
    const subB = obsB.subscribe(() => {});
    assertEquals(
        env.delegate.observations.length, 2,
        'Should have 2 independent delegate observations');
    subA.unsubscribe();
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);

    assertEquals(
        env.delegate.observations.length, 1,
        'Only one observation should be removed');
    assertEquals(
        env.delegate.observations[0]!.tabId, 'tabB',
        'tabB observation should remain');
    subB.unsubscribe();
  });

  test('closing the receiver completes the observable', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    let completed = false;
    obs.subscribe({
      complete() {
        completed = true;
      },
      next() {},
    });

    // Access the private receiver and close it to simulate a pipe closure from
    // the host
    const receiver = (obs as any).receiver;
    assertTrue(!!receiver, 'Receiver should be established after subscription');
    receiver.close();

    assertTrue(
        completed,
        'Observable should have completed when the receiver was closed');

    // Wait for prune
    await sleep(env.delegate.unsubscribeDelay + 1);
    await sleep(0);
    assertEquals(
        env.delegate.observations.length, 0,
        'Subscription should be cleaned up');
  });

  test(
      're-subscribing during complete() callback yields a fresh observable',
      () => {
        const env = createEnvironment();
        const obs1 = env.obs.getObservableByTabId('123');

        let reSubscribedObs: any = null;
        obs1.subscribe({
          complete() {
            // Re-subscribe immediately during the complete notification!
            reSubscribedObs = env.obs.getObservableByTabId('123');
          },
          next() {},
        });

        obs1.complete();

        assertNotEquals(
            obs1, reSubscribedObs,
            'Should have received a fresh observable instance');
      });
});
