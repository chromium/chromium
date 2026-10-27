// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GlicClientStorageImpl} from '//webui-test/glic/glic_api_impl/glic_api_client.js';
import type {ClientStorageHandler} from '//webui-test/glic/glic_api_impl/glic_api_client.js';
import {TabObservable, TabObservableSet} from '//webui-test/glic/glic_api_impl/observable_set_by_tab_id.js';
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

interface PendingSetCall {
  key: string;
  value: string|null;
  resolve: () => void;
}

class FakeClientStorageHandler implements ClientStorageHandler {
  setCalls: PendingSetCall[] = [];
  clearCalls: Array<{resolve: () => void}> = [];

  setClientStorageItem(key: string, value: string|null): Promise<void> {
    const {promise, resolve} = Promise.withResolvers<void>();
    this.setCalls.push({key, value, resolve});
    return promise;
  }

  clearClientStorage(): Promise<void> {
    const {promise, resolve} = Promise.withResolvers<void>();
    this.clearCalls.push({resolve});
    return promise;
  }
}

suite('GlicClientStorageImpl', () => {
  test('basic synchronous operations', () => {
    const handler = new FakeClientStorageHandler();
    const storage = new GlicClientStorageImpl(handler);

    storage.initialize({a: '1', b: '2'});
    assertEquals(storage.length, 2);
    assertEquals(storage.getItem('a'), '1');
    assertEquals(storage.getItem('b'), '2');
    assertEquals(storage.getItem('missing'), null);
    assertEquals(storage.key(0), 'a');
    assertEquals(storage.key(1), 'b');
    assertEquals(storage.key(-1), null);
    assertEquals(storage.key(2), null);

    storage.setItem('c', '3');
    assertEquals(storage.length, 3);
    assertEquals(storage.getItem('c'), '3');
    assertEquals(handler.setCalls.length, 1);
    assertEquals(handler.setCalls[0]!.key, 'c');
    assertEquals(handler.setCalls[0]!.value, '3');

    storage.removeItem('a');
    assertEquals(storage.length, 2);
    assertEquals(storage.getItem('a'), null);
    assertEquals(handler.setCalls.length, 2);
    assertEquals(handler.setCalls[1]!.key, 'a');
    assertEquals(handler.setCalls[1]!.value, null);

    storage.clear();
    assertEquals(storage.length, 0);
    assertEquals(handler.clearCalls.length, 1);
  });

  test('remote updates apply when no local mutations are pending', () => {
    const handler = new FakeClientStorageHandler();
    const storage = new GlicClientStorageImpl(handler);
    storage.initialize({k1: 'v1', k2: 'v2'});

    storage.onRemoteItemChanged('k1', 'v1_remote');
    assertEquals(storage.getItem('k1'), 'v1_remote');

    storage.onRemoteItemChanged('k3', 'v3_remote');
    assertEquals(storage.getItem('k3'), 'v3_remote');

    storage.onRemoteItemChanged('k2', null);
    assertEquals(storage.getItem('k2'), null);

    storage.onRemoteCleared();
    assertEquals(storage.length, 0);
  });

  test(
      'ignores remote item changes for key with in-flight mutation',
      async () => {
        const handler = new FakeClientStorageHandler();
        const storage = new GlicClientStorageImpl(handler);
        storage.initialize({k1: 'initial', k2: 'initial2'});

        storage.setItem('k1', 'local_v1');
        assertEquals(handler.setCalls.length, 1);

        // Racing remote update for k1 should be ignored while RPC is in flight.
        storage.onRemoteItemChanged('k1', 'stale_remote');
        assertEquals(storage.getItem('k1'), 'local_v1');
        storage.onRemoteItemChanged('k1', null);
        assertEquals(storage.getItem('k1'), 'local_v1');

        // Remote update for a different key k2 should still be applied.
        storage.onRemoteItemChanged('k2', 'remote_v2');
        assertEquals(storage.getItem('k2'), 'remote_v2');

        // Resolve the in-flight setClientStorageItem RPC.
        handler.setCalls[0]!.resolve();
        await Promise.resolve();
        await Promise.resolve();

        // Subsequent remote updates for k1 should now apply.
        storage.onRemoteItemChanged('k1', 'fresh_remote');
        assertEquals(storage.getItem('k1'), 'fresh_remote');
      });

  test('overlapping mutations on the same key wait for all RPCs', async () => {
    const handler = new FakeClientStorageHandler();
    const storage = new GlicClientStorageImpl(handler);

    storage.setItem('k1', 'v1');
    storage.setItem('k1', 'v2');
    assertEquals(handler.setCalls.length, 2);

    // Resolve first RPC while second is still in flight.
    handler.setCalls[0]!.resolve();
    await Promise.resolve();
    await Promise.resolve();

    // Remote update must still be ignored until the second RPC finishes.
    storage.onRemoteItemChanged('k1', 'stale_after_first');
    assertEquals(storage.getItem('k1'), 'v2');

    // Resolve second RPC.
    handler.setCalls[1]!.resolve();
    await Promise.resolve();
    await Promise.resolve();

    // Remote update now applies.
    storage.onRemoteItemChanged('k1', 'v3_remote');
    assertEquals(storage.getItem('k1'), 'v3_remote');
  });

  test('ignores remote item changes while clear is in flight', async () => {
    const handler = new FakeClientStorageHandler();
    const storage = new GlicClientStorageImpl(handler);
    storage.initialize({k1: 'v1'});

    storage.clear();
    assertEquals(handler.clearCalls.length, 1);

    // Stale remote item change arriving before clear is acknowledged is
    // ignored.
    storage.onRemoteItemChanged('k1', 'stale_remote');
    assertEquals(storage.length, 0);

    handler.clearCalls[0]!.resolve();
    await Promise.resolve();
    await Promise.resolve();

    // Remote item change after clear resolves is applied.
    storage.onRemoteItemChanged('k1', 'post_clear_remote');
    assertEquals(storage.getItem('k1'), 'post_clear_remote');
  });

  test('onRemoteCleared preserves unacknowledged local mutations', async () => {
    const handler = new FakeClientStorageHandler();
    const storage = new GlicClientStorageImpl(handler);
    storage.initialize({committed: 'old', deletedPending: 'old2'});

    storage.setItem('pendingSet', 'new_val');
    storage.removeItem('deletedPending');

    // Remote clear arrives while pendingSet and deletedPending RPCs are in
    // flight.
    storage.onRemoteCleared();

    // Committed key is cleared, pendingSet is preserved, deletedPending stays
    // absent.
    assertEquals(storage.getItem('committed'), null);
    assertEquals(storage.getItem('pendingSet'), 'new_val');
    assertEquals(storage.getItem('deletedPending'), null);
    assertEquals(storage.length, 1);

    handler.setCalls[0]!.resolve();
    handler.setCalls[1]!.resolve();
    await Promise.resolve();
    await Promise.resolve();
  });

  test('setItem then clear then setItem handles RPC completions', async () => {
    const handler = new FakeClientStorageHandler();
    const storage = new GlicClientStorageImpl(handler);

    storage.setItem('k1', 'before_clear');
    storage.clear();
    storage.setItem('k1', 'after_clear');
    assertEquals(handler.setCalls.length, 2);
    assertEquals(handler.clearCalls.length, 1);

    // First setItem (from before clear) resolves; it must not remove the
    // post-clear pending entry for k1.
    handler.setCalls[0]!.resolve();
    handler.clearCalls[0]!.resolve();
    await Promise.resolve();
    await Promise.resolve();

    // Second setItem is still in flight, so racing remote update is
    // ignored.
    storage.onRemoteItemChanged('k1', 'stale_remote');
    assertEquals(storage.getItem('k1'), 'after_clear');

    // Resolve second setItem; now remote updates apply.
    handler.setCalls[1]!.resolve();
    await Promise.resolve();
    await Promise.resolve();

    storage.onRemoteItemChanged('k1', 'final_remote');
    assertEquals(storage.getItem('k1'), 'final_remote');
  });
});
