// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabObservable, TabObservableSet} from '//webui-test/glic/glic_api_impl/client/observable_set_by_tab_id.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {sleep, waitUntilEqual} from './test_helpers.js';

const TEST_UNSUBSCRIBE_DELAY = 10;

interface CurrentSubscription {
  tabId: string;
}

class TestTabObservable extends TabObservable<string> {
  connected = false;
  receiver?: {close: () => void};

  constructor(
      tabId: string, private env: TestEnvironment, onComplete: () => void) {
    super(tabId, onComplete, /*unsubscribeDelay=*/ TEST_UNSUBSCRIBE_DELAY);
  }

  protected override isConnected(): boolean {
    return this.connected;
  }

  protected override connectToSource(): void {
    this.connected = true;
    this.receiver = {
      close: () => {
        this.complete();
      },
    };
    this.env.activeSubscriptions.push({tabId: this.tabId});
  }

  protected override disconnectFromSource(): void {
    if (this.connected) {
      this.connected = false;
      this.receiver = undefined;
      this.env.activeSubscriptions =
          this.env.activeSubscriptions.filter(s => s.tabId !== this.tabId);
    }
  }
}

class TestEnvironment {
  activeSubscriptions: CurrentSubscription[] = [];
  obs = new TabObservableSet<string, TestTabObservable>(
      (tabId, onComplete) => new TestTabObservable(tabId, this, onComplete));
}

suite('TabObservableSet', () => {
  function createEnvironment(): TestEnvironment {
    return new TestEnvironment();
  }

  test('send with no observers', () => {
    const env = createEnvironment();
    // Does nothing.
    env.obs.getObservableByTabId('4').assignAndSignal('HI');
    assertEquals(env.activeSubscriptions.length, 0);
    assertEquals(env.obs.getObservableByTabId('4').getCurrentValue(), 'HI');
  });

  test('subscribe to tab id', () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');
    assertEquals(
        obs.getCurrentValue(), undefined, 'Initial value is incorrect');
    let notifiedCount = 0;
    obs.subscribe((value) => {
      notifiedCount++;
      assertEquals(value, 'HI', 'Notified value is incorrect');
    });
    assertEquals(env.activeSubscriptions.length, 1);
    assertEquals(env.activeSubscriptions[0]!.tabId, '123');
    obs.assignAndSignal('HI');
    assertEquals(notifiedCount, 1, 'Subscriber was not notified');
    assertEquals(obs.getCurrentValue(), 'HI', 'getCurrentValue() is incorrect');
  });

  test('emits updates to multiple and late subscribers', () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    const received1: string[] = [];
    const received2: string[] = [];
    const received3: string[] = [];

    const sub1 = obs.subscribe(v => received1.push(v));
    const sub2 = obs.subscribe(v => received2.push(v));

    obs.assignAndSignal('value1');
    assertEquals(1, received1.length);
    assertEquals('value1', received1[0]);
    assertEquals(1, received2.length);
    assertEquals('value1', received2[0]);

    // Late subscriber should immediately receive current value.
    const sub3 = obs.subscribe(v => received3.push(v));
    assertEquals(1, received3.length);
    assertEquals('value1', received3[0]);

    obs.assignAndSignal('value2');
    assertEquals(2, received1.length);
    assertEquals('value2', received1[1]);
    assertEquals(2, received2.length);
    assertEquals('value2', received2[1]);
    assertEquals(2, received3.length);
    assertEquals('value2', received3[1]);

    sub1.unsubscribe();
    sub2.unsubscribe();
    sub3.unsubscribe();
  });

  test('complete removes subscription from set', () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');
    let completed = false;
    obs.subscribe({
      complete() {
        completed = true;
      },
      next() {},
    });
    assertEquals(
        env.activeSubscriptions.length, 1, 'Subscription was not created');
    assertEquals(env.activeSubscriptions[0]!.tabId, '123');

    obs.complete();
    assertTrue(completed, 'complete() was not called');
    assertTrue(
        obs.isStopped(), 'Observable should be stopped after complete()');
    assertEquals(
        env.activeSubscriptions.length, 0, 'Subscription was not removed');

    // Getting the tab again yields a new instance since it completed.
    const newObs = env.obs.getObservableByTabId('123');
    assertNotEquals(
        obs, newObs,
        'Subsequent lookup should yield a new observable instance');
    assertTrue(!newObs.isStopped(), 'New observable must not be stopped');
  });

  test('subscribe after unsubscribe before disconnect delay', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    const sub1 = obs.subscribe(() => {});
    assertEquals(
        env.activeSubscriptions.length, 1, 'observation was not created');
    obs.assignAndSignal('val1');

    sub1.unsubscribe();

    // Subscribe before the original subscription is disconnected. This should
    // keep the connection active and replay the current value.
    const received2: string[] = [];
    const sub2 = obs.subscribe(v => received2.push(v));
    assertEquals(received2.length, 1);
    assertEquals(received2[0], 'val1');

    await sleep(TEST_UNSUBSCRIBE_DELAY + 5);
    assertEquals(
        env.activeSubscriptions.length, 1,
        'just one observation after second subscribe');
    assertTrue(
        !obs.isStopped(),
        'observable should not be stopped while resubscribed');

    sub2.unsubscribe();
    await waitUntilEqual(() => env.activeSubscriptions.length, 0);
    assertTrue(
        !obs.isStopped(), 'observable should not be stopped after disconnect');
    assertEquals(
        obs.getCurrentValue(), undefined,
        'stored value should be cleared after disconnect');
  });

  test('cached observable reconnects after disconnect delay', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    const received1: string[] = [];
    const sub1 = obs.subscribe(v => received1.push(v));
    assertEquals(
        env.activeSubscriptions.length, 1, 'first observation was not created');
    obs.assignAndSignal('stale_val');
    assertEquals(received1.length, 1);
    assertEquals(obs.getCurrentValue(), 'stale_val');

    sub1.unsubscribe();
    await waitUntilEqual(() => env.activeSubscriptions.length, 0);
    assertTrue(
        !obs.isStopped(), 'observable must not be stopped after disconnect');
    assertEquals(
        obs.getCurrentValue(), undefined,
        'stored value must be cleared after disconnect');

    // Subscribe after the disconnect delay has passed. The cached observable
    // should reconnect to the source and must not synchronously replay the
    // stale value from the previous connection.
    const sameObs = env.obs.getObservableByTabId('123');
    assertEquals(
        obs, sameObs, 'Expected same observable instance to be cached');
    assertTrue(!sameObs.isStopped(), 'Cached observable must not be stopped');

    const received2: string[] = [];
    const sub2 = sameObs.subscribe(v => received2.push(v));
    assertEquals(
        env.activeSubscriptions.length, 1,
        'second observation was not created');
    assertEquals(
        received2.length, 0,
        'stale value must not be synchronously replayed upon re-subscribing');

    sameObs.assignAndSignal('fresh_val');
    assertEquals(received2.length, 1);
    assertEquals(received2[0], 'fresh_val');

    sub2.unsubscribe();
    await waitUntilEqual(() => env.activeSubscriptions.length, 0);
    assertTrue(
        !sameObs.isStopped(),
        'observable must not be stopped after second disconnect');
    assertEquals(
        sameObs.getCurrentValue(), undefined,
        'stored value must be cleared after second disconnect');
  });

  test('multiple concurrent subscribers (deduplication)', async () => {
    const env = createEnvironment();
    const obs = env.obs.getObservableByTabId('123');

    // First subscriber
    const sub1 = obs.subscribe(() => {});
    assertEquals(
        env.activeSubscriptions.length, 1, 'First sub should trigger connect');

    // Second subscriber
    assertEquals(
        obs, env.obs.getObservableByTabId('123'), 'Should get same observer');
    const sub2 = obs.subscribe(() => {});
    assertEquals(
        env.activeSubscriptions.length, 1,
        'Second sub should not trigger duplicate connect');

    // First unsubscribes
    sub1.unsubscribe();
    await sleep(TEST_UNSUBSCRIBE_DELAY + 5);
    assertEquals(
        env.activeSubscriptions.length, 1,
        'Connection should remain active while second sub exists');

    // Second unsubscribes
    sub2.unsubscribe();
    await waitUntilEqual(() => env.activeSubscriptions.length, 0);
    assertEquals(
        env.activeSubscriptions.length, 0,
        'Should disconnect after last sub unsubscribes');
  });

  test('requesting a tab after complete yields a new observable', () => {
    const env = createEnvironment();
    const obs1 = env.obs.getObservableByTabId('foo');
    const sub1 = obs1.subscribe(() => {});

    obs1.complete();

    const obs2 = env.obs.getObservableByTabId('foo');
    assertNotEquals(
        obs1, obs2, 'Expected new observable instance, but got the same one');
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
        env.activeSubscriptions.length, 2,
        'Should have 2 independent observations');
    subA.unsubscribe();
    await waitUntilEqual(() => env.activeSubscriptions.length, 1);

    assertEquals(
        env.activeSubscriptions.length, 1,
        'Only one observation should be disconnected');
    assertEquals(
        env.activeSubscriptions[0]!.tabId, 'tabB',
        'tabB observation should remain');
    subB.unsubscribe();
    await waitUntilEqual(() => env.activeSubscriptions.length, 0);
  });

  test('closing the receiver completes the observable', () => {
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
    // the host.
    const receiver = (obs as any).receiver;
    assertTrue(!!receiver, 'Receiver should be established after subscription');
    receiver.close();

    assertTrue(
        completed,
        'Observable should have completed when the receiver was closed');
    assertTrue(obs.isStopped(), 'Observable should be stopped');
    assertEquals(
        env.activeSubscriptions.length, 0, 'Subscription should be cleaned up');

    const newObs = env.obs.getObservableByTabId('123');
    assertNotEquals(
        obs, newObs, 'Expected a new observable instance after receiver close');
    assertTrue(!newObs.isStopped(), 'New observable must not be stopped');
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
