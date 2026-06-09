// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ObservableSetByTabIdDelegate, PostMessageRemote, RequestMessage, WebClientHost} from 'chrome://glic/glic.js';
import {IdGenerator, ObservableSetByTabId, PostMessageRouterImpl, WebClientHostDef} from 'chrome://glic/glic.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

class StubSender {
  sentMessages: RequestMessage[] = [];
  postMessage(message: any, _targetOrigin: string, _transfer?: Transferable[]):
      void {
    this.sentMessages.push(message);
  }
}

interface TestEnvironment {
  sender: PostMessageRemote<WebClientHost>;
  delegate: TestDelegate;
  idGenerator: IdGenerator;
  obs: ObservableSetByTabId<string>;
}

interface CurrentSubscription {
  observationId: number;
  tabId: string;
}

class TestDelegate implements ObservableSetByTabIdDelegate {
  readonly unsubscribeDelay = 1;
  observations: CurrentSubscription[] = [];
  subscribe(
      _sender: PostMessageRemote<any>, observationId: number,
      tabId: string): void {
    this.observations.push({observationId, tabId});
  }

  unsubscribe(
      _sender: PostMessageRemote<any>, observationId: number,
      tabId: string): void {
    this.observations = this.observations.filter((sub) => {
      return sub.observationId !== observationId || sub.tabId !== tabId;
    });
  }
}

function sleep(timeout: number): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, timeout));
}

suite('ObservableSetByTabId', () => {
  function createEnvironment(): TestEnvironment {
    const stubSender = new StubSender();
    const router = new PostMessageRouterImpl(
        'origin', 'senderId', stubSender, 'logPrefix', false);
    const sender = router.newPipeWithRemote(WebClientHostDef).remote;
    const delegate = new TestDelegate();
    const idGenerator = new IdGenerator();
    const obs = new ObservableSetByTabId<string>(delegate, sender, idGenerator);
    return {sender, delegate, idGenerator, obs};
  }

  test('send with no observers', () => {
    const env = createEnvironment();
    // Does nothing.
    env.obs.assignAndSignal(4, 'HI');
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
    env.obs.assignAndSignal(env.delegate.observations[0]!.observationId, 'HI');
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

    env.obs.completeObservable(env.delegate.observations[0]!.observationId);
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

    // Subscribe after the original subscription is pruned. This should reuse
    // the first observation.
    const sub2 = obs.subscribe(() => {});
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
        const observationId = env.delegate.observations[0]!.observationId;

        // Calling completeObservable queues a prune operation.
        env.obs.completeObservable(observationId);

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
});
