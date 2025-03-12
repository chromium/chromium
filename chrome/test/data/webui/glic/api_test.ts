// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import {PanelStateKind} from '/glic/glic_api/glic_api.js';
import type {GlicBrowserHost, GlicWebClient, Observable, PanelState} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from './api_boot.js';

// Creates a queue of promises from an observable.
class SequencedSubscriber<T> {
  private signals: Array<PromiseWithResolvers<T>> = [];
  private readIndex = 0;
  private writeIndex = 0;

  constructor(observable: Observable<T>) {
    observable.subscribe(this.change.bind(this));
  }
  next(): Promise<T> {
    return this.getSignal(this.readIndex++).promise;
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

// A dummy web client.
class WebClient implements GlicWebClient {
  host?: GlicBrowserHost;
  firstOpened = Promise.withResolvers<void>();

  async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    this.host = glicBrowserHost;
  }

  async notifyPanelWillOpen(_panelState: PanelState): Promise<void> {
    this.firstOpened.resolve();
  }
}

let webClientPromise: Promise<WebClient>|undefined;

async function main() {
  const {promise, resolve} = Promise.withResolvers<WebClient>();
  webClientPromise = promise;
  const registry = await createGlicHostRegistryOnLoad();
  const webClient = new WebClient();
  registry.registerWebClient(webClient);
  resolve(webClient);
}

// Test cases here correspond to test cases in glic_api_uitest.cc.
// Since these tests run in the webview, this test can't use normal deps like
// mocha or chai assert.
class ApiTests {
  constructor(private client: WebClient, private host: GlicBrowserHost) {}

  async setUp() {
    await this.client.firstOpened.promise;
  }

  async testCreateTab() {
    assertTrue(!!this.host.createTab);
    // Open a tab pointing to test.html.
    const url = location.href;
    const data = await this.host.createTab(url, {openInBackground: false});
    assertEquals(data.url, url);
  }

  async testOpenGlicSettingsPage() {
    assertTrue(!!this.host.openGlicSettingsPage);
    this.host.openGlicSettingsPage();
    // There is a problem with InProcessBrowserTest::QuitBrowsers(). Opening a
    // browser at the same time as exiting a test results in QuitBrowsers()
    // never exiting. This sleep avoids this problem.
    await sleep(500);
  }

  async testClosePanel() {
    assertTrue(!!this.host.closePanel);
    await this.host.closePanel();
  }

  async testAttachPanel() {
    // Test starts in attached mode.
    await this.waitForPanelState(PanelStateKind.DETACHED);
    assertTrue(!!this.host.attachPanel);
    this.host.attachPanel();
    await this.waitForPanelState(PanelStateKind.ATTACHED);
  }

  async testDetachPanel() {
    // Test starts in detached mode.
    await this.waitForPanelState(PanelStateKind.ATTACHED);
    assertTrue(!!this.host.detachPanel);
    this.host.detachPanel();
    await this.waitForPanelState(PanelStateKind.DETACHED);
  }

  async testShowProfilePicker() {
    assertTrue(!!this.host.showProfilePicker);
    this.host.showProfilePicker();
    // There is a problem with InProcessBrowserTest::QuitBrowsers(). Opening the
    // profile picker at the same time as exiting a test results in
    // QuitBrowsers() never exiting. This sleep avoids this problem.
    await sleep(500);
  }

  async testPanelActive() {
    assertTrue(!!this.host.panelActive);
    const signal = Promise.withResolvers<boolean>();
    this.host.panelActive().subscribe(v => signal.resolve(v));
    assertTrue(await signal.promise);
  }

  async testCanAttachPanel() {
    assertTrue(!!this.host.canAttachPanel);
    const canAttach = new SequencedSubscriber(this.host.canAttachPanel());
    // When subscribing to this value, an initial update is guaranteed to be
    // emited.
    assertTrue(await canAttach.next());
  }

  async testGetFocusedTabStateV2() {
    assertTrue(!!this.host.getFocusedTabStateV2);
    const sequence = new SequencedSubscriber(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertTrue(!!focus.focusedTab, 'Should be a focused tab');
    assertTrue(
        focus.focusedTab.url.endsWith('glic/test.html'),
        `url=${focus.focusedTab.url}`);
    assertEquals('Test Page', focus.focusedTab.title);
  }

  async waitForPanelState(kind: PanelStateKind): Promise<void> {
    assertTrue(!!this.host.getPanelState);
    const sequence = new SequencedSubscriber(this.host.getPanelState());
    while (true) {
      const state = await sequence.next();
      if (state.kind === kind) {
        return;
      }
      console.info(`Got panel state ${state.kind} != ${kind}`);
    }
  }
}

async function runApiTest(name: string): Promise<string> {
  try {
    const client = await webClientPromise;
    assertTrue(!!client);
    const host = client.host;
    assertTrue(!!host);

    const fixture = new ApiTests(client, host);
    await fixture.setUp();
    assertTrue(!!(fixture as any)[name], 'Test case not found');
    await (fixture as any)[name]();
  } catch (e) {
    if (e instanceof Error) {
      console.error(e.stack);
    }
    return `fail: ${e}`;
  }
  return 'pass';
}

(window as any).runApiTest = runApiTest;

type ComparableValue = string|number|undefined|null;

function assertTrue(x: boolean, message?: string): asserts x {
  if (!x) {
    throw new Error(`assertTrue failed: ${x} is not true. ${message ?? ''}`);
  }
}

function assertEquals(
    a: ComparableValue, b: ComparableValue, message?: string) {
  if (a !== b) {
    throw new Error(`assertEquals(${a}, ${b}) failed. ${message ?? ''}`);
  }
}

function sleep(timeoutMs: number) {
  return new Promise((resolve) => {
    window.setTimeout(resolve, timeoutMs);
  });
}

main();
