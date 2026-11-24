// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageMetadata as PageMetadataMojo} from '//webui-test/glic/ai_page_content_metadata.mojom-webui.js';
import {DrivePickerErrorReason as DrivePickerErrorReasonMojo, HostCapability as HostCapabilityMojo} from '//webui-test/glic/glic.mojom-webui.js';
import type {WebClientHandlerRemote, WebClientInitialState} from '//webui-test/glic/glic.mojom-webui.js';
import {DrivePickerErrorReason, ExecuteToolErrorReason} from '//webui-test/glic/glic_api/glic_api.js';
import type {PageMetadata} from '//webui-test/glic/glic_api/glic_api.js';
import {driveFileSelectionToClient, drivePickerOptionsFromClient, drivePickerResultToClient, numberToClient} from '//webui-test/glic/glic_api_impl/conversions.js';
import {GlicBrowserHostImpl, GlicClientStorageImpl, PageMetadataObservableSet} from '//webui-test/glic/glic_api_impl/glic_api_client.js';
import type {ClientStorageHandler, PageMetadataHandler} from '//webui-test/glic/glic_api_impl/glic_api_client.js';
import {TabObservable, TabObservableSet} from '//webui-test/glic/glic_api_impl/observable_set_by_tab_id.js';
import {GlicBrowserHostTools} from '//webui-test/glic/glic_api_impl/tools/tools_client.js';
import {AiOverlayToolsReceiver} from '//webui-test/glic/tools.mojom-webui.js';
import type {AiOverlayToolsInterface, AiOverlayToolsPendingReceiver} from '//webui-test/glic/tools.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

suite('DrivePickerApiClient', () => {
  test('numberToClient converts bigint, number, and null', () => {
    assertEquals(numberToClient(4096n), 4096);
    assertEquals(numberToClient(0n), 0);
    assertEquals(numberToClient(42), 42);
    assertEquals(numberToClient(null), undefined);
  });

  test(
      'driveFileSelectionToClient converts bigint size and optional fields',
      () => {
        assertDeepEquals(
            driveFileSelectionToClient({
              id: 'file-1',
              title: 'Report.pdf',
              mimeType: 'application/pdf',
              sizeBytes: 4096n,
              resourceKey: 'rk-123',
            }),
            {
              id: 'file-1',
              title: 'Report.pdf',
              mimeType: 'application/pdf',
              sizeBytes: 4096,
              resourceKey: 'rk-123',
            });

        assertDeepEquals(
            driveFileSelectionToClient({
              id: 'doc-1',
              title: 'Doc',
              mimeType: 'application/vnd.google-apps.document',
              sizeBytes: 0n,
              resourceKey: null,
            }),
            {
              id: 'doc-1',
              title: 'Doc',
              mimeType: 'application/vnd.google-apps.document',
              sizeBytes: 0,
              resourceKey: undefined,
            });
      });

  test('drivePickerResultToClient converts union variants', () => {
    assertDeepEquals(
        drivePickerResultToClient({
          files: [{
            id: 'abc',
            title: 'Deck.pptx',
            mimeType: 'application/pdf',
            sizeBytes: 2048n,
            resourceKey: 'rk',
          }],
        }),
        {
          files: [{
            id: 'abc',
            title: 'Deck.pptx',
            mimeType: 'application/pdf',
            sizeBytes: 2048,
            resourceKey: 'rk',
          }],
        });
    assertDeepEquals(drivePickerResultToClient({files: []}), {files: []});
    assertDeepEquals(
        drivePickerResultToClient(
            {errorReason: DrivePickerErrorReasonMojo.kUnknown}),
        {errorReason: DrivePickerErrorReason.UNKNOWN});
    assertDeepEquals(
        drivePickerResultToClient(
            {errorReason: DrivePickerErrorReasonMojo.kAccountMismatch}),
        {errorReason: DrivePickerErrorReason.ACCOUNT_MISMATCH});
    assertThrows(() => drivePickerResultToClient({}));
  });

  test('drivePickerOptionsFromClient defaults missing options', () => {
    assertDeepEquals(
        drivePickerOptionsFromClient({accountId: '12345'}),
        {accountId: '12345'});
    assertDeepEquals(drivePickerOptionsFromClient(undefined), {accountId: ''});
  });

  test('showDrivePicker returns converted selections on success', async () => {
    let receivedOptions: {accountId: string}|undefined;
    const fakeHost = {
      handler: {
        showDrivePicker: (options: {accountId: string}) => {
          receivedOptions = options;
          return Promise.resolve({
            result: {
              files: [
                {
                  id: 'abc',
                  title: 'Deck.pptx',
                  mimeType: 'application/pdf',
                  sizeBytes: 2048n,
                  resourceKey: 'rk',
                },
              ],
            },
          });
        },
      },
    };
    const result = await GlicBrowserHostImpl.prototype.showDrivePicker!.call(
        fakeHost as unknown as GlicBrowserHostImpl, {accountId: 'gaia-1'});
    assertDeepEquals(receivedOptions, {accountId: 'gaia-1'});
    assertDeepEquals(result, {
      files: [
        {
          id: 'abc',
          title: 'Deck.pptx',
          mimeType: 'application/pdf',
          sizeBytes: 2048,
          resourceKey: 'rk',
        },
      ],
    });
  });

  test('showDrivePicker returns empty files array on cancel', async () => {
    const fakeHost = {
      handler: {
        showDrivePicker: () => Promise.resolve({result: {files: []}}),
      },
    };
    const result = await GlicBrowserHostImpl.prototype.showDrivePicker!.call(
        fakeHost as unknown as GlicBrowserHostImpl, {accountId: 'gaia-1'});
    assertDeepEquals(result, {files: []});
  });

  test('showDrivePicker returns errorReason on error', async () => {
    const fakeHost = {
      handler: {
        showDrivePicker: () => Promise.resolve({
          result: {errorReason: DrivePickerErrorReasonMojo.kAccountMismatch},
        }),
      },
    };
    const result = await GlicBrowserHostImpl.prototype.showDrivePicker!.call(
        fakeHost as unknown as GlicBrowserHostImpl, {accountId: 'gaia-1'});
    assertDeepEquals(
        result, {errorReason: DrivePickerErrorReason.ACCOUNT_MISMATCH});
  });
});

suite('GlicBrowserHostTools', () => {
  function makeInitialState(enableTools: boolean): WebClientInitialState {
    return {
      hostCapabilities: enableTools ? [HostCapabilityMojo.kChromeTools] : [],
    } as unknown as WebClientInitialState;
  }

  function createFakeToolsEnv() {
    let createCalls = 0;
    let goBackCalls = 0;
    let goForwardCalls = 0;
    const receivers: AiOverlayToolsReceiver[] = [];

    const stub = new Proxy({}, {
                   get(_target, prop) {
                     return () => {
                       if (prop === 'goBack') {
                         goBackCalls++;
                       } else if (prop === 'goForward') {
                         goForwardCalls++;
                       }
                       return Promise.resolve({});
                     };
                   },
                 }) as unknown as AiOverlayToolsInterface;

    const handler = {
      createAiOverlayTools(receiver: AiOverlayToolsPendingReceiver) {
        createCalls++;
        const bound = new AiOverlayToolsReceiver(stub);
        bound.$.bindHandle(receiver.handle);
        receivers.push(bound);
      },
    } as unknown as WebClientHandlerRemote;

    return {
      handler,
      getCreateCalls: () => createCalls,
      getGoBackCalls: () => goBackCalls,
      getGoForwardCalls: () => goForwardCalls,
      closeAllReceivers: () => {
        for (const r of receivers) {
          r.$.close();
        }
      },
    };
  }

  test('defers Mojo pipe binding until first valid executeTool', async () => {
    const env = createFakeToolsEnv();
    const hostTools = new GlicBrowserHostTools();
    hostTools.initialize(makeInitialState(true), env.handler);

    assertTrue(Boolean(hostTools.tools));
    const toolsHost = hostTools.tools!();
    const tools = await toolsHost.getChromeTools!();
    assertTrue(Boolean(tools && tools.length > 0));
    assertEquals(0, env.getCreateCalls());

    const unknownRes = await toolsHost.executeTool!('nonexistent_tool', '{}');
    assertEquals(ExecuteToolErrorReason.TOOL_NOT_FOUND, unknownRes.errorReason);
    assertEquals(0, env.getCreateCalls());

    const badJsonRes = await toolsHost.executeTool!('go_back', '{invalid');
    assertEquals(
        ExecuteToolErrorReason.INVALID_ARGUMENTS, badJsonRes.errorReason);
    assertEquals(0, env.getCreateCalls());

    const nullJsonRes = await toolsHost.executeTool!('go_back', 'null');
    assertEquals(
        ExecuteToolErrorReason.INVALID_ARGUMENTS, nullJsonRes.errorReason);
    assertEquals(0, env.getCreateCalls());

    const missingArgRes = await toolsHost.executeTool!('perform_search', '{}');
    assertEquals(
        ExecuteToolErrorReason.INVALID_ARGUMENTS, missingArgRes.errorReason);
    assertEquals(0, env.getCreateCalls());

    const [res1, res2] = await Promise.all([
      toolsHost.executeTool!('go_back', '{}'),
      toolsHost.executeTool!('go_forward', '{}'),
    ]);
    assertEquals('{}', res1.result);
    assertEquals('{}', res2.result);
    assertEquals(1, env.getCreateCalls());
    assertEquals(1, env.getGoBackCalls());
    assertEquals(1, env.getGoForwardCalls());

    env.closeAllReceivers();
  });

  test(
      'returns undefined and disables tools when capability is absent',
      async () => {
        const env = createFakeToolsEnv();
        const hostTools = new GlicBrowserHostTools();
        const preInitToolsHost = hostTools.tools!();

        hostTools.initialize(makeInitialState(false), env.handler);
        assertEquals(undefined, hostTools.tools);
        assertEquals(undefined, await preInitToolsHost.getChromeTools!());

        const res = await preInitToolsHost.executeTool!('go_back', '{}');
        assertEquals(ExecuteToolErrorReason.EXECUTION_FAILED, res.errorReason);
        assertEquals(0, env.getCreateCalls());
      });
});

interface PendingSubscribeCall {
  tabId: number;
  names: string[];
  resolve: (result: {success: boolean}) => void;
  reject: (reason?: unknown) => void;
}

class FakePageMetadataHandler implements PageMetadataHandler {
  calls: PendingSubscribeCall[] = [];

  subscribeToPageMetadata(tabId: number, names: string[]):
      Promise<{success: boolean}> {
    const {promise, resolve, reject} =
        Promise.withResolvers<{success: boolean}>();
    this.calls.push({tabId, names, resolve, reject});
    return promise;
  }
}

function makeMojoMetadata(url: string, author?: string): PageMetadataMojo {
  return {
    frameMetadata: [{
      url,
      metaTags: author !== undefined ? [{name: 'author', content: author}] : [],
      hasMediaTranscripts: false,
    }],
  };
}

suite('PageMetadataObservableSet', () => {
  test('throws on empty names and deduplicates observable per tabId', () => {
    const handler = new FakePageMetadataHandler();
    const metadataSet = new PageMetadataObservableSet(handler);

    assertThrows(() => metadataSet.getObservable('123', []));

    const obs1 = metadataSet.getObservable('123', ['author']);
    const obs2 = metadataSet.getObservable('123', ['description']);
    assertEquals(obs1, obs2);
    assertEquals(handler.calls.length, 0);
  });

  test(
      'resets stored value on unsubscribe and does not replay stale metadata',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);
        const obs = metadataSet.getObservable('123', ['author']);

        const received1: PageMetadata[] = [];
        const sub1 = obs.subscribe(v => received1.push(v));
        assertEquals(handler.calls.length, 1);
        assertEquals(handler.calls[0]!.tabId, 123);
        assertDeepEquals(handler.calls[0]!.names, ['author']);
        handler.calls[0]!.resolve({success: true});
        await Promise.resolve();

        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://a.example/', 'George'));
        assertEquals(received1.length, 1);
        assertEquals(
            obs.getCurrentValue()?.frameMetadata[0]?.metaTags[0]?.content,
            'George');

        // Unsubscribing the last subscriber immediately resets the stored value
        // and tears down the browser-side observer, while keeping the
        // observable active and cached for future subscriptions.
        sub1.unsubscribe();
        assertFalse(obs.isStopped());
        assertEquals(obs.getCurrentValue(), undefined);
        assertEquals(handler.calls.length, 2);
        assertEquals(handler.calls[1]!.tabId, 123);
        assertDeepEquals(handler.calls[1]!.names, []);
        handler.calls[1]!.resolve({success: true});
        await Promise.resolve();

        // Re-subscribing (both via getObservable and the cached instance) must
        // not synchronously emit the old page's metadata.
        const cachedObs = metadataSet.getObservable('123', ['author']);
        assertEquals(obs, cachedObs);

        const received2: PageMetadata[] = [];
        const sub2 = cachedObs.subscribe(v => received2.push(v));
        assertEquals(received2.length, 0);
        assertEquals(handler.calls.length, 3);
        assertEquals(handler.calls[2]!.tabId, 123);
        assertDeepEquals(handler.calls[2]!.names, ['author']);
        handler.calls[2]!.resolve({success: true});
        await Promise.resolve();

        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://b.example/', 'Ruth'));
        assertEquals(received2.length, 1);
        assertEquals(
            received2[0]!.frameMetadata[0]!.metaTags[0]!.content, 'Ruth');

        sub2.unsubscribe();
      });

  test(
      'ignores in-flight metadata updates arriving after unsubscribe',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);
        const obs = metadataSet.getObservable('123', ['author']);

        const sub1 = obs.subscribe(() => {});
        handler.calls[0]!.resolve({success: true});
        await Promise.resolve();

        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://a.example/', 'George'));
        sub1.unsubscribe();
        assertEquals(obs.getCurrentValue(), undefined);

        // Simulate an in-flight NotifyPageMetadataChanged arriving before the
        // browser processes SubscribeToPageMetadata(123, []).
        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://a.example/', 'StaleInFlight'));
        assertEquals(obs.getCurrentValue(), undefined);

        handler.calls[1]!.resolve({success: true});
        await Promise.resolve();

        const received: PageMetadata[] = [];
        const sub2 = obs.subscribe(v => received.push(v));
        assertEquals(received.length, 0);
        sub2.unsubscribe();
      });

  test('completes and removes observable when tab is destroyed', async () => {
    const handler = new FakePageMetadataHandler();
    const metadataSet = new PageMetadataObservableSet(handler);
    const obs1 = metadataSet.getObservable('123', ['author']);

    let completed = false;
    let reSubscribedObs: unknown = null;
    obs1.subscribe({
      next() {},
      complete() {
        completed = true;
        reSubscribedObs = metadataSet.getObservable('123', ['author']);
      },
    });
    assertEquals(handler.calls.length, 1);
    handler.calls[0]!.resolve({success: true});
    await Promise.resolve();

    metadataSet.notifyPageMetadataChanged(
        123, makeMojoMetadata('https://a.example/', 'George'));
    assertTrue(obs1.getCurrentValue() !== undefined);

    // Null metadata signals tab destruction.
    metadataSet.notifyPageMetadataChanged(123, null);
    assertTrue(completed);
    assertTrue(obs1.isStopped());
    assertEquals(obs1.getCurrentValue(), undefined);
    // Should not send a redundant unsubscribe RPC when completing.
    assertEquals(handler.calls.length, 1);
    assertNotEquals(obs1, reSubscribedObs);
  });

  test(
      'completes and removes observable when subscribe fails after resubscribe',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);
        const obs1 = metadataSet.getObservable('123', ['author']);

        const sub1 = obs1.subscribe(() => {});
        handler.calls[0]!.resolve({success: true});
        await Promise.resolve();
        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://a.example/', 'George'));

        sub1.unsubscribe();
        handler.calls[1]!.resolve({success: true});
        await Promise.resolve();

        // Tab was closed while unsubscribed; re-subscribing fails on host.
        const received: PageMetadata[] = [];
        let completed = false;
        obs1.subscribe({
          next: v => received.push(v),
          complete() {
            completed = true;
          },
        });
        assertEquals(received.length, 0);
        assertEquals(handler.calls.length, 3);
        handler.calls[2]!.resolve({success: false});
        await Promise.resolve();

        assertTrue(completed);
        assertTrue(obs1.isStopped());
        assertEquals(received.length, 0);
        // Should not send a redundant unsubscribe RPC after failure.
        assertEquals(handler.calls.length, 3);
        assertNotEquals(obs1, metadataSet.getObservable('123', ['author']));
      });

  test(
      'multiple concurrent subscribers share subscription until last leaves',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);
        const obs = metadataSet.getObservable('123', ['author']);

        const received1: PageMetadata[] = [];
        const received2: PageMetadata[] = [];
        const sub1 = obs.subscribe(v => received1.push(v));
        assertEquals(handler.calls.length, 1);
        assertDeepEquals(handler.calls[0]!.names, ['author']);
        handler.calls[0]!.resolve({success: true});
        await Promise.resolve();

        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://a.example/', 'George'));
        assertEquals(received1.length, 1);

        // Second subscriber joins: no duplicate subscribe RPC, and current
        // value is immediately replayed.
        const sub2 = obs.subscribe(v => received2.push(v));
        assertEquals(handler.calls.length, 1);
        assertEquals(received2.length, 1);
        assertEquals(
            received2[0]!.frameMetadata[0]!.metaTags[0]!.content, 'George');

        // First subscriber leaves while second remains active: stored value is
        // kept and no unsubscribe RPC is sent.
        sub1.unsubscribe();
        assertEquals(handler.calls.length, 1);
        assertEquals(
            obs.getCurrentValue()?.frameMetadata[0]?.metaTags[0]?.content,
            'George');

        metadataSet.notifyPageMetadataChanged(
            123, makeMojoMetadata('https://a.example/', 'Ruth'));
        assertEquals(received1.length, 1);
        assertEquals(received2.length, 2);
        assertEquals(
            received2[1]!.frameMetadata[0]!.metaTags[0]!.content, 'Ruth');

        // Last subscriber leaves: stored value resets and unsubscribe is sent.
        sub2.unsubscribe();
        assertEquals(obs.getCurrentValue(), undefined);
        assertEquals(handler.calls.length, 2);
        assertDeepEquals(handler.calls[1]!.names, []);
      });

  test(
      'copies names array so caller mutations do not affect subscriptions',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);
        const callerNames = ['author', 'description'];
        const obs = metadataSet.getObservable('123', callerNames);

        // Mutating (including clearing) the caller's array must not turn
        // subsequent subscriptions into unsubscriptions.
        callerNames.length = 0;

        const sub1 = obs.subscribe(() => {});
        assertEquals(handler.calls.length, 1);
        assertDeepEquals(handler.calls[0]!.names, ['author', 'description']);
        handler.calls[0]!.resolve({success: true});
        await Promise.resolve();

        sub1.unsubscribe();
        assertEquals(handler.calls.length, 2);
        assertDeepEquals(handler.calls[1]!.names, []);
        handler.calls[1]!.resolve({success: true});
        await Promise.resolve();

        callerNames.push('keywords');
        const sub2 = obs.subscribe(() => {});
        assertEquals(handler.calls.length, 3);
        assertDeepEquals(handler.calls[2]!.names, ['author', 'description']);
        sub2.unsubscribe();
      });

  test('defers resubscription until pending unsubscribe resolves', async () => {
    const handler = new FakePageMetadataHandler();
    const metadataSet = new PageMetadataObservableSet(handler);
    const obs = metadataSet.getObservable('123', ['author']);

    const sub1 = obs.subscribe(() => {});
    handler.calls[0]!.resolve({success: true});
    await Promise.resolve();

    metadataSet.notifyPageMetadataChanged(
        123, makeMojoMetadata('https://a.example/', 'George'));

    // Unsubscribe and immediately re-subscribe before unsubscribe resolves.
    sub1.unsubscribe();
    assertEquals(handler.calls.length, 2);
    assertDeepEquals(handler.calls[1]!.names, []);

    const received2: PageMetadata[] = [];
    const sub2 = obs.subscribe(v => received2.push(v));
    // Re-subscription RPC must wait for the pending unsubscribe to finish.
    assertEquals(handler.calls.length, 2);
    assertEquals(received2.length, 0);

    // Stale in-flight update from the prior browser observer arriving
    // before the unsubscribe completes must be ignored.
    metadataSet.notifyPageMetadataChanged(
        123, makeMojoMetadata('https://a.example/', 'StaleInFlight'));
    assertEquals(received2.length, 0);
    assertEquals(obs.getCurrentValue(), undefined);

    // Once unsubscribe resolves, the deferred re-subscription is sent and
    // subsequent updates are delivered to the new subscriber.
    handler.calls[1]!.resolve({success: true});
    await Promise.resolve();
    assertEquals(handler.calls.length, 3);
    assertDeepEquals(handler.calls[2]!.names, ['author']);

    metadataSet.notifyPageMetadataChanged(
        123, makeMojoMetadata('https://b.example/', 'Ruth'));
    assertEquals(received2.length, 1);
    assertEquals(received2[0]!.frameMetadata[0]!.metaTags[0]!.content, 'Ruth');
    handler.calls[2]!.resolve({success: true});
    await Promise.resolve();

    // Subscribing and unsubscribing again while an unsubscribe is still in
    // flight must not send extra subscribe or unsubscribe RPCs.
    sub2.unsubscribe();
    assertEquals(handler.calls.length, 4);
    assertDeepEquals(handler.calls[3]!.names, []);

    const sub3 = obs.subscribe(() => {});
    sub3.unsubscribe();
    assertEquals(handler.calls.length, 4);

    handler.calls[3]!.resolve({success: true});
    await Promise.resolve();
    assertEquals(handler.calls.length, 4);
  });

  test(
      'completes waiting resubscriber when pending unsubscribe fails',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);
        const obs = metadataSet.getObservable('123', ['author']);

        const sub1 = obs.subscribe(() => {});
        handler.calls[0]!.resolve({success: true});
        await Promise.resolve();

        sub1.unsubscribe();
        let completed = false;
        obs.subscribe({
          next() {},
          complete() {
            completed = true;
          },
        });

        handler.calls[1]!.resolve({success: false});
        await Promise.resolve();

        assertTrue(completed);
        assertTrue(obs.isStopped());
        assertEquals(handler.calls.length, 2);
        assertNotEquals(obs, metadataSet.getObservable('123', ['author']));
      });

  test(
      'completes on subscribe and pending unsubscribe rejections', async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);

        const obs1 = metadataSet.getObservable('123', ['author']);
        let obs1Completed = false;
        obs1.subscribe({
          next() {},
          complete() {
            obs1Completed = true;
          },
        });
        handler.calls[0]!.reject(new Error('pipe closed'));
        await Promise.resolve();
        assertTrue(obs1Completed);
        assertTrue(obs1.isStopped());
        assertEquals(handler.calls.length, 1);

        const obs2 = metadataSet.getObservable('123', ['author']);
        assertNotEquals(obs1, obs2);
        const sub = obs2.subscribe(() => {});
        handler.calls[1]!.resolve({success: true});
        await Promise.resolve();

        sub.unsubscribe();
        let obs2Completed = false;
        obs2.subscribe({
          next() {},
          complete() {
            obs2Completed = true;
          },
        });
        handler.calls[2]!.reject(new Error('pipe closed'));
        await Promise.resolve();
        assertTrue(obs2Completed);
        assertTrue(obs2.isStopped());
        assertEquals(handler.calls.length, 3);
      });

  test(
      'completes during pending unsubscribe on subscribe failure or null',
      async () => {
        const handler = new FakePageMetadataHandler();
        const metadataSet = new PageMetadataObservableSet(handler);

        const obs1 = metadataSet.getObservable('123', ['author']);
        const sub1 = obs1.subscribe(() => {});
        sub1.unsubscribe();
        let obs1Completed = false;
        obs1.subscribe({
          next() {},
          complete() {
            obs1Completed = true;
          },
        });
        assertEquals(handler.calls.length, 2);

        handler.calls[0]!.resolve({success: false});
        await Promise.resolve();
        assertTrue(obs1Completed);

        handler.calls[1]!.resolve({success: true});
        await Promise.resolve();
        assertEquals(handler.calls.length, 2);

        const obs2 = metadataSet.getObservable('123', ['author']);
        assertNotEquals(obs1, obs2);
        const sub2 = obs2.subscribe(() => {});
        handler.calls[2]!.resolve({success: true});
        await Promise.resolve();

        sub2.unsubscribe();
        let obs2Completed = false;
        obs2.subscribe({
          next() {},
          complete() {
            obs2Completed = true;
          },
        });
        metadataSet.notifyPageMetadataChanged(123, null);
        assertTrue(obs2Completed);

        handler.calls[3]!.resolve({success: true});
        await Promise.resolve();
        assertEquals(handler.calls.length, 4);
      });
});
