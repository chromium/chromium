// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://glic/browser_proxy.js';
import {PrepareForClientResult, ProfileReadyState} from 'chrome://glic/glic.mojom-webui.js';
import type {ApiHostEmbedder} from 'chrome://glic/glic_api_impl/host/glic_api_host.js';
import {WebClientState} from 'chrome://glic/glic_api_impl/host/glic_api_host.js';
import type {GlicWebviewLoadState} from 'chrome://glic/glic_webview_loader.js';
import {GlicWebviewLoader, GlicWebviewLoadErrorReason, GlicWebviewLoadStatus} from 'chrome://glic/glic_webview_loader.js';
import type {ObservableValueReadOnly, Subscriber} from 'chrome://glic/observable.js';
import {ObservableValue} from 'chrome://glic/observable.js';
import type {PageType, WebviewControllerInterface} from 'chrome://glic/webview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {FakePageHandler, FakeWebviewController} from './fakes.js';

class TestGlicWebviewLoader extends GlicWebviewLoader {
  autoPageLoad?: PageType = 'regular';
  private testWebClientState =
      ObservableValue.withValue(WebClientState.UNINITIALIZED);

  override createWebviewController(): WebviewControllerInterface {
    const controller = new FakeWebviewController();
    // Make web client state controllable through `TestGlicWebviewLoader`,
    // so tests don't need to wait for the controller to be created before
    // manipulating it.
    controller.webClientState = this.testWebClientState;
    if (this.autoPageLoad) {
      this.webviewPageCommit(this.autoPageLoad);
    }
    return controller;
  }

  get webviewController(): FakeWebviewController|undefined {
    return this.webview as FakeWebviewController | undefined;
  }

  setWebClientState(state: WebClientState) {
    this.testWebClientState.assignAndSignal(state);
  }
}

class FakeBrowserProxy implements BrowserProxy {
  pageHandler = new FakePageHandler();
}

class FakeApiHostEmbedder implements ApiHostEmbedder {
  onGuestResizeRequest(_size: {width: number, height: number}): void {}
  enableDragResize(_enabled: boolean): void {}
  webClientReady(): void {}
}

export function sleep(timeoutMs: number): Promise<void> {
  return new Promise((resolve) => {
    window.setTimeout(resolve, timeoutMs);
  });
}

async function waitUntil(condition: () => boolean): Promise<void> {
  while (!condition()) {
    await sleep(1);
  }
}

// Waits for a promise to resolve. If the timeout is reached first, throws an
// exception. Note this is useful because if the test times out in the normal
// way, we do not receive a very useful error.
async function waitFor<T>(value: Promise<T>, message?: string): Promise<T> {
  const timeoutResult = Symbol();
  const result =
      await Promise.race([value, sleep(1000).then(() => timeoutResult)]);
  if (result === timeoutResult) {
    throw new Error(`waitFor timed out. ${message ?? ''}`);
  }
  return value;
}

function stateToString(state: GlicWebviewLoadState|undefined) {
  if (state === undefined) {
    return 'undefined';
  }
  if (state.errorReason) {
    return `{errorReason: ${
        GlicWebviewLoadErrorReason[state.errorReason]} status: ${
        GlicWebviewLoadStatus[state.status]}}`;
  }
  return `{status: ${GlicWebviewLoadStatus[state.status]}}`;
}
function assertStateEquals(
    want: GlicWebviewLoadStatus|GlicWebviewLoadState|undefined,
    got: GlicWebviewLoadState|undefined) {
  if (typeof (want) === 'number') {
    assertStateEquals({status: want}, got);
    return;
  }
  assertEquals(stateToString(want), stateToString(got));
}

function assertStateIsError(
    wantErrorReason: GlicWebviewLoadErrorReason,
    got: GlicWebviewLoadState|undefined) {
  assertStateEquals(
      {
        status: GlicWebviewLoadStatus.ERROR,
        errorReason: wantErrorReason,
      },
      got);
}

// Creates a queue of promises from an observable.
export class SequencedSubscriber<T> {
  private signals: Array<PromiseWithResolvers<T>> = [];
  private readIndex = 0;
  private writeIndex = 0;
  private subscriber: Subscriber;

  // The last value read from `next()`, or undefined if none was read.
  current: {some: T}|undefined;

  // A promise that resolves when the observable is completed.
  readonly completed: Promise<void>;

  constructor(observable: ObservableValueReadOnly<T>) {
    const completedResolvers = Promise.withResolvers<void>();
    this.completed = completedResolvers.promise;
    this.subscriber = observable.subscribeObserver({
      next: this.change.bind(this),
      complete: completedResolvers.resolve,
    });
  }
  async next(): Promise<T> {
    // Wrapping the returned value with `waitFor` improves failure logs
    // on timeout.
    this.current = {
      some: await waitFor(this.getSignal(this.readIndex++).promise),
    };
    return this.current.some;
  }

  /** Returns true if all values have been read. */
  isEmpty(): boolean {
    return this.readIndex >= this.writeIndex;
  }
  unsubscribe() {
    this.subscriber.unsubscribe();
  }
  waitForValue(targetValue: T) {
    return this.waitFor(v => v === targetValue);
  }
  async waitFor(condition: (v: T) => boolean): Promise<T> {
    let lastValueSaw: {some: T}|undefined = undefined;
    if (this.current !== undefined) {
      if (condition(this.current.some)) {
        return this.current.some;
      }
      lastValueSaw = {some: this.current.some};
    }

    while (true) {
      let val;
      try {
        val = await this.next();
      } catch (e) {
        if (lastValueSaw !== undefined) {
          console.warn(`waitFor() failed, last value saw was ${
              JSON.stringify(lastValueSaw)}`);
        } else {
          console.warn(`waitFor() failed, saw no values emitted`);
        }
        throw e;
      }
      if (condition(val)) {
        return val;
      }
      lastValueSaw = {some: val};
      console.info(`waitFor saw and ignored ${JSON.stringify(val)}`);
    }
  }
  private change(val: T) {
    this.getSignal(this.writeIndex++).resolve(val);
  }
  private getSignal(index: number) {
    while (this.signals.length <= index) {
      this.signals.push(Promise.withResolvers<T>());
    }
    return this.signals[index]!;
  }
}

export function observeSequence<T>(observable: ObservableValueReadOnly<T>):
    SequencedSubscriber<T> {
  return new SequencedSubscriber(observable);
}

suite('GlicWebviewLoaderTest', () => {
  let browserProxy = new FakeBrowserProxy();
  const onlineMonitor = ObservableValue.withValue(true);
  const embedder = new FakeApiHostEmbedder();
  const container = document.createElement('div');
  setup(() => {
    browserProxy = new FakeBrowserProxy();
    loadTimeData.resetForTesting({
      preLoadingTimeMs: 50,
      minLoadingTimeMs: 50,
      maxLoadingTimeMs: 50,
      enableDebug: false,
      enableWebClientUnresponsiveMetrics: false,
    });

    // loadTimeData.resetForTesting({});
    onlineMonitor.assignAndSignal(true);
  });

  async function progressThroughNormalLoad(loader: TestGlicWebviewLoader):
      Promise<void> {
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);
    switch (loader.currentStatus()) {
      case GlicWebviewLoadStatus.LOADING:
        break;
      case GlicWebviewLoadStatus.RESPONSIVE:  // prerequisites are already met.
        return;
      default:
        assertFalse(
            true, `Not trying to load. state=${loader.currentStatus()}`);
    }
    await waitUntil(() => loader.webviewController !== undefined);
    loader.setWebClientState(WebClientState.RESPONSIVE);
    assertStateEquals(
        GlicWebviewLoadStatus.RESPONSIVE, loader.getState().getCurrentValue());
  }

  test('Create', () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    assertEquals(GlicWebviewLoadStatus.NOT_LOADED, loader.currentStatus());
  });

  test('Create and load', async () => {
    // Create the loader and force it through a normal loading pattern.
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);

    // Trigger unload.
    loader.setWantLoad(false);
    await observeSequence(loader.getState())
        .waitFor(s => s.status === GlicWebviewLoadStatus.NOT_LOADED);
  });

  test('No load while offline', async () => {
    onlineMonitor.assignAndSignal(false);
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    const statusSequence = observeSequence(loader.getState());
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.NOT_ONLINE);
  });

  test('Load after regaining connection', async () => {
    onlineMonitor.assignAndSignal(false);
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    const statusSequence = observeSequence(loader.getState());
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.NOT_ONLINE);

    onlineMonitor.assignAndSignal(true);
    loader.setWebClientState(WebClientState.RESPONSIVE);

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.RESPONSIVE);
  });

  test('Stay loaded if disconnected', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);

    // Go offline, it should remain loaded.
    onlineMonitor.assignAndSignal(false);
    assertEquals(GlicWebviewLoadStatus.RESPONSIVE, loader.currentStatus());
  });

  test('Client stops working after load', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);

    // Trigger a guest error.
    loader.setWebClientState(WebClientState.ERROR);

    observeSequence(loader.getState())
        .waitFor(
            s => s.status === GlicWebviewLoadStatus.ERROR &&
                s.errorReason === GlicWebviewLoadErrorReason.UNKNOWN);
  });

  test('Client error during load', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);
    // Trigger a client error.
    await waitUntil(() => loader.webviewController !== undefined);
    loader.setWebClientState(WebClientState.ERROR);

    await observeSequence(loader.getState())
        .waitFor(
            s => s.status === GlicWebviewLoadStatus.ERROR &&
                s.errorReason === GlicWebviewLoadErrorReason.UNKNOWN);
  });

  test('Profile not ready', () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.setWantLoad(true);
    assertStateEquals(
        GlicWebviewLoadStatus.NOT_LOADED, loader.getState().getCurrentValue());

    loader.setProfileReadyState(ProfileReadyState.kDisabledByAdmin);
    assertStateIsError(
        GlicWebviewLoadErrorReason.DISABLED_BY_ADMIN,
        loader.getState().getCurrentValue());

    loader.setProfileReadyState(ProfileReadyState.kIneligible);
    assertStateIsError(
        GlicWebviewLoadErrorReason.PROFILE_INELIGIBLE,
        loader.getState().getCurrentValue());

    loader.setProfileReadyState(ProfileReadyState.kUnknownError);
    assertStateIsError(
        GlicWebviewLoadErrorReason.PROFILE_INELIGIBLE,
        loader.getState().getCurrentValue());

    loader.setProfileReadyState(ProfileReadyState.kSignInRequired);
    assertStateIsError(
        GlicWebviewLoadErrorReason.BLOCKED_BY_NEED_SIGN_IN,
        loader.getState().getCurrentValue());

    loader.setProfileReadyState(ProfileReadyState.kReady);
    assertStateEquals(
        GlicWebviewLoadStatus.LOADING, loader.getState().getCurrentValue());
  });

  test('Profile not ready after load', () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    progressThroughNormalLoad(loader);

    loader.setProfileReadyState(ProfileReadyState.kDisabledByAdmin);
    assertStateIsError(
        GlicWebviewLoadErrorReason.DISABLED_BY_ADMIN,
        loader.getState().getCurrentValue());
    assertEquals(undefined, loader.webviewController);
  });

  test('should handle prepare for client error resyncing cookies', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    const statusSequence = observeSequence(loader.getState());
    browserProxy.pageHandler.prepareForClient = () => {
      return Promise.resolve(
          {result: PrepareForClientResult.kErrorResyncingCookies});
    };

    // Trigger the load
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    // Wait for the state to transition to ERROR with the correct reason

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.BLOCKED_BY_SYNC_ERROR);
  });

  test('should handle prepare for client requires sign in', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    const statusSequence = observeSequence(loader.getState());

    browserProxy.pageHandler.prepareForClient = () => {
      return Promise.resolve({result: PrepareForClientResult.kRequiresSignIn});
    };

    // Trigger the load
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason ===
                GlicWebviewLoadErrorReason.BLOCKED_BY_NEED_SIGN_IN);
  });

  test('should stop loading due to timeout', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    const statusSequence = observeSequence(loader.getState());

    // Start loading.
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    // Wait for the loader to enter the LOADING state
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);

    await waitUntil(() => loader.webviewController !== undefined);
    // Trigger timeout.
    loader.stopLoadingDueToTimeout();

    // Verify the timeout was handled.
    assertStateEquals(
        GlicWebviewLoadStatus.NOT_LOADED, loader.getState().getCurrentValue());
  });

  test('page load guest error', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.allowLoginPages = false;
    const statusSequence = observeSequence(loader.getState());

    // Start loading.
    loader.autoPageLoad = 'guestError';
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.AT_GUEST_ERROR_PAGE);
  });

  test('page load error', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.allowLoginPages = false;
    const statusSequence = observeSequence(loader.getState());

    // Start loading.
    loader.autoPageLoad = 'loadError';
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.PAGE_LOAD_ERROR);
  });

  test('login page reached and not allowed', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.allowLoginPages = false;
    const statusSequence = observeSequence(loader.getState());

    // Start loading.
    loader.autoPageLoad = 'login';
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    // Wait for the loader to enter the LOADING state
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.LOGIN_PAGE_REACHED);
  });

  test('login page reached and allowed', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.allowLoginPages = true;
    const statusSequence = observeSequence(loader.getState());

    // Start loading.
    loader.autoPageLoad = 'login';
    loader.setWantLoad(true);
    loader.setProfileReadyState(ProfileReadyState.kReady);

    // Wait for the loader to enter the LOADING state
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.AT_LOGIN_PAGE);
  });

  test('client responsiveness forwarded as load status', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);

    loader.setWebClientState(WebClientState.UNRESPONSIVE);
    assertStateEquals(
        GlicWebviewLoadStatus.UNRESPONSIVE,
        loader.getState().getCurrentValue());

    loader.setWebClientState(WebClientState.RESPONSIVE);
    assertStateEquals(
        GlicWebviewLoadStatus.RESPONSIVE, loader.getState().getCurrentValue());
  });

  test('client permanantly unresponsive', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);

    loader.setWebClientState(WebClientState.UNRESPONSIVE);
    assertStateEquals(
        GlicWebviewLoadStatus.UNRESPONSIVE,
        loader.getState().getCurrentValue());

    loader.setWebClientState(WebClientState.ERROR);
    assertStateIsError(
        GlicWebviewLoadErrorReason.UNKNOWN,
        loader.getState().getCurrentValue());
  });

  test('Commit after load forces reload', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);

    const statusSequence = observeSequence(loader.getState());
    loader.setWebClientState(WebClientState.UNINITIALIZED);
    loader.webviewPageCommit('regular');

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);
    loader.setWebClientState(WebClientState.RESPONSIVE);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.RESPONSIVE);
  });


  test('webview denited by admin', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    loader.setWantLoad(true);

    loader.setProfileReadyState(ProfileReadyState.kReady);
    const statusSequence = observeSequence(loader.getState());
    loader.setWebClientState(WebClientState.UNINITIALIZED);
    loader.webviewDeniedByAdmin();

    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.LOADING);
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.DISABLED_BY_ADMIN);
  });

  test('webview error after load', async () => {
    const loader = new TestGlicWebviewLoader(
        container, browserProxy, embedder, onlineMonitor);
    await progressThroughNormalLoad(loader);
    const statusSequence = observeSequence(loader.getState());
    loader.webviewError('foo');
    await statusSequence.waitFor(
        s => s.status === GlicWebviewLoadStatus.ERROR &&
            s.errorReason === GlicWebviewLoadErrorReason.UNKNOWN);
  });
});
