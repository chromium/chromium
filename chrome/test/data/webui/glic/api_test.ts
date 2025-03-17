// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import {GetTabContextErrorReason, PanelStateKind, ScrollToErrorReason, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {GetTabContextError, GlicBrowserHost, GlicWebClient, Observable, PanelOpeningData, ScrollToError} from '/glic/glic_api/glic_api.js';

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

function observeSequence<T>(observable: Observable<T>): SequencedSubscriber<T> {
  return new SequencedSubscriber(observable);
}

// A dummy web client.
class WebClient implements GlicWebClient {
  host?: GlicBrowserHost;
  firstOpened = Promise.withResolvers<void>();

  async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    this.host = glicBrowserHost;
  }

  async notifyPanelWillOpen(_panelOpeningData: PanelOpeningData):
      Promise<void> {
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
    const canAttach = observeSequence(this.host.canAttachPanel());
    // When subscribing to this value, an initial update is guaranteed to be
    // emited.
    assertTrue(await canAttach.next());
  }

  async testGetFocusedTabStateV2() {
    assertTrue(!!this.host.getFocusedTabStateV2);
    const sequence = observeSequence(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertTrue(!!focus.focusedTab, 'Should be a focused tab');
    assertTrue(
        focus.focusedTab.url.endsWith('glic/test.html'),
        `url=${focus.focusedTab.url}`);
    assertEquals('Test Page', focus.focusedTab.title);
  }

  async testGetContextFromFocusedTabWithoutPermission() {
    assertTrue(!!this.host?.getContextFromFocusedTab);
    await this.host?.setTabContextPermissionState(false);

    try {
      await this.host.getContextFromFocusedTab?.({});
    } catch (e) {
      assertEquals(
          GetTabContextErrorReason.PERMISSION_DENIED,
          (e as GetTabContextError).reason);
    }
  }

  async testGetContextFromFocusedTabWithNoRequestedData() {
    await this.host?.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab?.({});
    assertTrue(!!result);
    assertTrue(
        result.tabData.url.endsWith('glic/test.html') ?? false,
        `Tab data has unexpected url ${result.tabData.url}`);
    assertTrue(!result.annotatedPageData);
    assertTrue(!result.pdfDocumentData);
    assertTrue(!result.webPageData);
    assertTrue(!result.viewportScreenshot);
  }

  // TODO(harringtond): Add a test for a PDF.
  async testGetContextFromFocusedTabWithAllRequestedData() {
    await this.host?.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab?.({
      innerText: true,
      viewportScreenshot: true,
      annotatedPageContent: true,
      pdfData: true,
    });

    assertTrue(!!result);

    assertTrue(
        result.tabData.url.endsWith('glic/test.html') ?? false,
        `Tab data has unexpected url ${result.tabData.url}`);
    assertTrue(!result.pdfDocumentData);  // The page is not a PDF.
    assertTrue(!!result.webPageData);
    assertEquals(
        'This is a test page', result.webPageData.mainDocument.innerText);
    assertTrue(!!result.viewportScreenshot);
    assertTrue(
        (result.viewportScreenshot.data.byteLength ?? 0) > 0,
        `Expected viewport screenshot bytes, got ${
            result.viewportScreenshot.data.byteLength}`);
    assertTrue(result.viewportScreenshot.heightPixels > 0);
    assertTrue(result.viewportScreenshot.widthPixels > 0);
    assertEquals('image/jpeg', result.viewportScreenshot.mimeType);
    assertTrue(!!result.annotatedPageData);
    const annotatedPageContentSize =
        (await new Response(result.annotatedPageData.annotatedPageContent)
             .bytes())
            .length;
    assertTrue(annotatedPageContentSize > 1);
  }

  // TODO(harringtond): This is disabled because it hangs. Fix it.
  async testCaptureScreenshot() {
    assertTrue(!!this.host.captureScreenshot);
    const screenshot = await this.host.captureScreenshot?.();
    assertTrue(!!screenshot);
    assertTrue(screenshot.widthPixels > 0);
    assertTrue(screenshot.heightPixels > 0);
    assertTrue(screenshot.data.byteLength > 0);
    assertEquals(screenshot.mimeType, 'image/jpeg');
  }

  async testPermissionAccess() {
    assertTrue(!!this.host.getMicrophonePermissionState);
    assertTrue(!!this.host.getLocationPermissionState);
    assertTrue(!!this.host.getTabContextPermissionState);

    const microphoneState =
        observeSequence(this.host.getMicrophonePermissionState());
    const locationState =
        observeSequence(this.host.getLocationPermissionState());
    const tabContextState =
        observeSequence(this.host.getTabContextPermissionState());

    assertTrue(!await microphoneState.next());
    assertTrue(!await locationState.next());
    assertTrue(!await tabContextState.next());

    this.host.setMicrophonePermissionState(true);
    assertTrue(await microphoneState.next());

    this.host.setLocationPermissionState(true);
    assertTrue(await locationState.next());

    this.host.setTabContextPermissionState(true);
    assertTrue(await tabContextState.next());
  }

  async testGetUserProfileInfo() {
    assertTrue(!!this.host.getUserProfileInfo);
    const profileInfo = await this.host.getUserProfileInfo();

    assertEquals('', profileInfo.displayName);
    assertEquals('glic-test@example.com', profileInfo.email);
    assertEquals('', profileInfo.givenName);
    assertEquals(false, profileInfo.isManaged!);
    assertTrue((profileInfo.localProfileName?.length ?? 0) > 0);
  }

  async testRefreshSignInCookies() {
    assertTrue(!!this.host.refreshSignInCookies);

    await this.host.refreshSignInCookies();
  }

  async testSetContextAccessIndicator() {
    assertTrue(!!this.host.setContextAccessIndicator);

    await this.host.setContextAccessIndicator(true);
  }

  async testSetAudioDucking() {
    assertTrue(!!this.host.setAudioDucking);

    await this.host.setAudioDucking(true);
  }

  async testMetrics() {
    assertTrue(!!this.host.getMetrics);
    const metrics = this.host.getMetrics();
    assertTrue(!!metrics);
    assertTrue(!!metrics.onResponseRated);
    assertTrue(!!metrics.onUserInputSubmitted);
    assertTrue(!!metrics.onResponseStarted);
    assertTrue(!!metrics.onResponseStopped);
    assertTrue(!!metrics.onSessionTerminated);
    metrics.onResponseRated(true);
    metrics.onUserInputSubmitted(WebClientMode.AUDIO);
    metrics.onResponseStarted();
    metrics.onResponseStopped();
    metrics.onSessionTerminated();
  }

  async testScrollToFindsText() {
    assertTrue(!!this.host.scrollTo);
    await this.host.scrollTo(
        {selector: {exactText: {text: 'Test Page'}}, highlight: true});
  }

  async testScrollToNoMatchFound() {
    assertTrue(!!this.host.scrollTo);
    try {
      await this.host.scrollTo(
          {selector: {exactText: {text: 'Abracadabra'}}, highlight: true});
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.NO_MATCH_FOUND, (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  private async waitForPanelState(kind: PanelStateKind): Promise<void> {
    assertTrue(!!this.host.getPanelState);
    const sequence = observeSequence(this.host.getPanelState());
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

type ComparableValue = boolean|string|number|undefined|null;

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
