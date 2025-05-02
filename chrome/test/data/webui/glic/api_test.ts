// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import {PanelStateKind, ScrollToErrorReason, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {GlicBrowserHost, GlicHostRegistry, GlicWebClient, Observable, OpenPanelInfo, PanelOpeningData, ScrollToError, Subscriber} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from './api_boot.js';

function getTestName(): string|null {
  let testName = new URL(window.location.href).searchParams.get('test');
  if (testName?.startsWith('DISABLED_')) {
    testName = testName.substring('DISABLED_'.length);
  }
  return testName;
}

// Creates a queue of promises from an observable.
class SequencedSubscriber<T> {
  private signals: Array<PromiseWithResolvers<T>> = [];
  private readIndex = 0;
  private writeIndex = 0;
  private subscriber: Subscriber;

  constructor(observable: Observable<T>) {
    this.subscriber = observable.subscribe(this.change.bind(this));
  }
  next(): Promise<T> {
    return this.getSignal(this.readIndex++).promise;
  }
  unsubscribe() {
    this.subscriber.unsubscribe();
  }
  waitForValue(targetValue: T) {
    return this.waitFor(v => v === targetValue);
  }
  async waitFor(condition: (v: T) => boolean) {
    while (true) {
      const val = await this.next();
      if (condition(val)) {
        return;
      }
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

function observeSequence<T>(observable: Observable<T>): SequencedSubscriber<T> {
  return new SequencedSubscriber(observable);
}

// A dummy web client.
class WebClient implements GlicWebClient {
  host?: GlicBrowserHost;
  firstOpened = Promise.withResolvers<void>();
  initializedPromise = Promise.withResolvers<void>();

  async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    this.host = glicBrowserHost;
    this.initializedPromise.resolve();
  }

  async notifyPanelWillOpen(_panelOpeningData: PanelOpeningData):
      Promise<void|OpenPanelInfo> {
    this.firstOpened.resolve();
  }

  waitForFirstOpen(): Promise<void> {
    return this.firstOpened.promise;
  }

  waitForInitialize(): Promise<void> {
    return this.initializedPromise.promise;
  }
}

interface TestStepper {
  nextStep(data: any): Promise<void>;
}

const glicHostRegistry = Promise.withResolvers<GlicHostRegistry>();

class ApiTestFixtureBase {
  private clientValue?: WebClient;
  // Test parameters passed to `ExecuteJsTest()`. This is undefined until
  // ExecuteJsTest() is called.
  testParams: any;
  constructor(protected testStepper: TestStepper) {}

  // Sets up the web client. This is called when the web contents loads,
  // before `ExecuteJsTest()`.
  async setUpClient() {
    const registry = await glicHostRegistry.promise;
    const webClient = this.createWebClient();
    registry.registerWebClient(webClient);
    this.clientValue = webClient;
    assertTrue(!!this.clientValue);
  }

  // Performs setup for the test, called after `setUpClient()`.
  async setUpTest() {}

  // Creates the web client. Allows a fixture to use a different implementation.
  createWebClient() {
    return new WebClient();
  }

  get host(): GlicBrowserHost {
    const h = this.client.host;
    assertTrue(!!h);
    return h;
  }

  get client(): WebClient {
    assertTrue(!!this.clientValue);
    return this.clientValue;
  }
}

// Test cases here correspond to test cases in glic_api_uitest.cc.
// Since these tests run in the webview, this test can't use normal deps like
// mocha or chai assert.
class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  // Return to the C++ side, and wait for it to call ContinueJsTest() to
  // continue execution in the JS test. Optionally, pass data to the C++ side.
  private advanceToNextStep(data?: any): Promise<void> {
    return this.testStepper.nextStep(data);
  }

  // WARNING: Remember to update chrome/browser/glic/host/glic_api_uitest.cc
  // if you add a new test!

  async testDoNothing() {}

  async testAllTestsAreRegistered() {
    const allNames = [];
    for (const fixture of TEST_FIXTURES) {
      allNames.push(...Object.getOwnPropertyNames(fixture.prototype)
                        .filter(name => name.startsWith('test')));
    }
    await this.advanceToNextStep(allNames);
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

  // Verify that unsubscribing from an observable prevents future updates.
  async testUnsubscribeFromObservable() {
    await this.waitForPanelState(PanelStateKind.DETACHED);
    const panelStateSequence1 = observeSequence(this.host.getPanelState!());
    const panelStateSequence2 = observeSequence(this.host.getPanelState!());
    assertEquals(
        PanelStateKind.DETACHED, (await panelStateSequence1.next()).kind);
    assertEquals(
        PanelStateKind.DETACHED, (await panelStateSequence2.next()).kind);
    panelStateSequence2.unsubscribe();
    this.host.attachPanel!();
    assertEquals(
        PanelStateKind.ATTACHED, (await panelStateSequence1.next()).kind);
    assertEquals('no-change', await Promise.race([
      panelStateSequence2.next().then(state => state.kind),
      sleep(100).then(() => 'no-change'),
    ]));
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
    const activeSequence = observeSequence(this.host.panelActive());
    assertTrue(!!this.host.closePanel);
    await this.host.closePanel();
    assertTrue(await activeSequence.next());
    await this.advanceToNextStep();
    assertFalse(await activeSequence.next());
  }

  async testCanAttachPanel() {
    assertTrue(!!this.host.canAttachPanel);
    const canAttach = observeSequence(this.host.canAttachPanel());
    // When subscribing to this value, an initial update is guaranteed to be
    // emited.
    assertTrue(await canAttach.next());
  }

  async testIsBrowserOpen() {
    assertTrue(!!this.host.isBrowserOpen);
    const isBrowserOpen = observeSequence(this.host.isBrowserOpen());
    assertTrue(await isBrowserOpen.next());
    // Close the browser.
    await this.advanceToNextStep();
    assertTrue(!await isBrowserOpen.next());
  }

  async testEnableDragResize() {
    assertTrue(!!this.host.enableDragResize);

    await this.host.enableDragResize(true);
  }

  async testDisableDragResize() {
    assertTrue(!!this.host.enableDragResize);

    await this.host.enableDragResize(false);
  }

  async testGetFocusedTabState() {
    assertTrue(!!this.host.getFocusedTabState);
    const sequence = observeSequence(this.host.getFocusedTabState());
    const focus = await sequence.next();
    assertTrue(!!focus);
    assertTrue(focus.url.endsWith('glic/test.html'), `url=${focus.url}`);
    assertEquals('Test Page', focus.title);
  }

  async testGetFocusedTabStateV2() {
    assertTrue(!!this.host.getFocusedTabStateV2);
    const sequence = observeSequence(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertTrue(!!focus.hasFocus);
    assertTrue(
        focus.hasFocus.tabData.url.endsWith('glic/test.html'),
        `url=${focus.hasFocus.tabData.url}`);
    assertEquals('Test Page', focus.hasFocus.tabData.title);
    assertFalse(!!focus.hasNoFocus);
  }

  async testGetFocusedTabStateV2BrowserClosed() {
    assertTrue(!!this.host.getFocusedTabStateV2);
    const sequence = observeSequence(this.host.getFocusedTabStateV2());
    // Ignore the initial focus.
    await sequence.next();
    const focus = await sequence.next();
    assertFalse(!!focus.hasFocus);
    assertTrue(!!focus.hasNoFocus);
  }

  async testGetContextFromFocusedTabWithoutPermission() {
    assertTrue(!!this.host.getContextFromFocusedTab);
    await this.host.setTabContextPermissionState(false);

    try {
      await this.host.getContextFromFocusedTab?.({});
    } catch (e) {
      assertEquals(
          'tabContext failed: permission denied', (e as Error).message);
    }
  }

  async testGetContextFromFocusedTabWithNoRequestedData() {
    assertTrue(!!this.host.getContextFromFocusedTab);
    await this.host.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab({});
    assertTrue(!!result);
    assertTrue(
        result.tabData.url.endsWith('glic/test.html') ?? false,
        `Tab data has unexpected url ${result.tabData.url}`);
    assertFalse(!!result.annotatedPageData);
    assertFalse(!!result.pdfDocumentData);
    assertFalse(!!result.webPageData);
    assertFalse(!!result.viewportScreenshot);
  }

  // TODO(harringtond): Add a test for a PDF.
  async testGetContextFromFocusedTabWithAllRequestedData() {
    await this.host.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab?.({
      innerText: true,
      viewportScreenshot: true,
      annotatedPageContent: true,
      maxMetaTags: 32,
      pdfData: true,
    });

    assertTrue(!!result);

    assertTrue(
        result.tabData.url.endsWith('glic/test.html') ?? false,
        `Tab data has unexpected url ${result.tabData.url}`);
    assertFalse(!!result.pdfDocumentData);  // The page is not a PDF.
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

    // Check metadata.
    assertTrue(!!result.annotatedPageData.metadata);
    assertTrue(!!result.annotatedPageData.metadata.frameMetadata);
    assertEquals(result.annotatedPageData.metadata.frameMetadata.length, 1);
    const frameMetadata = result.annotatedPageData.metadata.frameMetadata[0];
    assertTrue(!!frameMetadata);
    const url: URL = new URL(frameMetadata.url);
    assertEquals(url.pathname, '/glic/test.html');
    assertEquals(frameMetadata.metaTags.length, 1);
    const metaTag = frameMetadata.metaTags[0];
    assertTrue(!!metaTag);
    assertEquals(metaTag.name, 'author');
    assertEquals(metaTag.content, 'George');
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

    assertFalse(await microphoneState.next());
    assertFalse(await locationState.next());
    assertFalse(await tabContextState.next());

    this.host.setMicrophonePermissionState(true);
    assertTrue(await microphoneState.next());

    this.host.setLocationPermissionState(true);
    assertTrue(await locationState.next());

    this.host.setTabContextPermissionState(true);
    assertTrue(await tabContextState.next());
  }

  async testGetOsHotkeyState() {
    assertTrue(!!this.host.getOsHotkeyState);
    const osHotkeyState = observeSequence(this.host.getOsHotkeyState());
    const initialHotkeyState = await osHotkeyState.next();
    assertEquals('<Ctrl>-<G>', initialHotkeyState.hotkey);
    await this.advanceToNextStep();
    const changedState = await osHotkeyState.next();
    assertEquals('<Ctrl>-<Shift>-<1>', changedState.hotkey);
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

  async testSetSyntheticExperimentState() {
    assertTrue(!!this.host.setSyntheticExperimentState);
    this.host.setSyntheticExperimentState('TestTrial', 'Enabled');
  }

  async testSetSyntheticExperimentStateMultiProfile() {
    assertTrue(!!this.host.setSyntheticExperimentState);
    this.host.setSyntheticExperimentState('TestTrial', 'Group1');
    this.host.setSyntheticExperimentState('TestTrial', 'Group2');
  }

  async testSetWindowDraggableAreas() {
    const draggableAreas = [{x: 10, y: 20, width: 30, height: 40}];
    assertTrue(!!this.host.setWindowDraggableAreas);
    await this.host.setWindowDraggableAreas(
        draggableAreas,
    );
    await this.advanceToNextStep(draggableAreas);
  }

  async testSetWindowDraggableAreasDefault() {
    assertTrue(!!this.host.setWindowDraggableAreas);
    await this.host.setWindowDraggableAreas([]);
  }

  async testSetMinimumWidgetSize() {
    assertTrue(!!this.host.setMinimumWidgetSize);
    const minSize = {width: 200, height: 100};
    await this.host.setMinimumWidgetSize(minSize.width, minSize.height);
    await this.advanceToNextStep(minSize);
  }

  // Test navigating successfully after client connection.
  async testNavigateToDifferentClientPage() {
    // This test function is run twice.
    const runCount: number = this.testParams;

    const url = new URL(window.location.href);
    // First time:
    if (runCount === 0) {
      url.searchParams.set('foobar', '1');
      (async () => {
        await sleep(100);
        location.href = url.toString();
      })();
      return;
    }

    // Second time:
    assertEquals(runCount, 1);
    assertEquals(url.searchParams.get('foobar'), '1');
  }

  // Test navigating unsuccessfully after client connection.
  async testNavigateToBadPage() {
    // This test function is run twice.
    const runCount: number = this.testParams;

    const url = new URL(window.location.href);
    // First time:
    if (runCount === 0) {
      // Close the panel so that it can be opened again later to trigger
      // loading the client.
      await this.host.closePanel!();
      // A regular web page with no client.
      url.pathname = '/test_data/page.html';
      (async () => {
        await sleep(100);
        location.href = url.toString();
      })();
      return;
    }

    // Second time:
    assertEquals(runCount, 1);
    assertEquals(url.pathname, '/glic/test.html');
  }

  private async waitForPanelState(kind: PanelStateKind): Promise<void> {
    assertTrue(!!this.host.getPanelState);
    await observeSequence(this.host.getPanelState())
        .waitFor(s => s.kind === kind);
  }
}

// Tests which do not wait for the panel to open before starting.
class ApiTestWithoutOpen extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForInitialize();
  }

  async testLoadWhileWindowClosed() {
    await observeSequence(this.host.panelActive()).waitForValue(false);
  }
}

type InitFailureType = 'error'|'timeout'|'none'|'reloadAfterInitialize';

// A web client that can fail initialize.
class WebClientThatFailsInitialize extends WebClient {
  constructor(private failWith: InitFailureType = 'error') {
    super();
  }

  override initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    if (this.failWith === 'error') {
      return Promise.reject(
          new Error('WebClientThatFailsInitialize.initialize'));
    }
    if (this.failWith === 'timeout') {
      return sleep(15000);
    }
    if (this.failWith === 'reloadAfterInitialize') {
      sleep(500).then(() => location.reload());
    }
    return super.initialize(glicBrowserHost);
  }
}

class ApiTestFailsToInitialize extends ApiTestFixtureBase {
  getTestParams(): {failWith?: InitFailureType} {
    return this.testParams ?? {};
  }

  override createWebClient(): WebClient {
    return new WebClientThatFailsInitialize(
        this.getTestParams().failWith ?? 'error');
  }

  // Defer setup until the test function is called, so that we can access
  // testParams when `createWebClient()` is called.
  override async setUpClient() {}

  async testInitializeFailsWindowClosed() {
    // Failing initialize will tear down this web contents. Deferring that here
    // so that our test can exit cleanly.
    sleep(100).then(() => super.setUpClient());
  }

  async testInitializeFailsWindowOpen() {
    // Failing initialize will tear down this web contents. Deferring that here
    // so that our test can exit cleanly.
    sleep(100).then(() => super.setUpClient());
  }

  async testReload() {
    // First run.
    if (this.getTestParams().failWith === 'reloadAfterInitialize') {
      sleep(100).then(() => super.setUpClient());
      return;
    }

    // Second run. Client should initialize and be opened.
    await super.setUpClient();
    await this.client.waitForFirstOpen();
  }

  async testInitializeFailsAfterReload() {
    sleep(100).then(() => super.setUpClient());
  }

  async testInitializeTimesOut() {
    await super.setUpClient();
  }
}

class WebClientThatOpensOnce extends WebClient {
  notifyPanelWillOpenCallCount = 0;
  override async notifyPanelWillOpen(panelOpeningData: PanelOpeningData):
      Promise<void> {
    this.notifyPanelWillOpenCallCount += 1;
    super.notifyPanelWillOpen(panelOpeningData);
  }
}

class NotifyPanelWillOpenTest extends ApiTestFixtureBase {
  override createWebClient(): WebClient {
    return new WebClientThatOpensOnce();
  }

  async testNotifyPanelWillOpenIsCalledOnce() {
    await sleep(100);
    assertEquals(
        (this.client as WebClientThatOpensOnce).notifyPanelWillOpenCallCount,
        1);
  }
}

class InitiallyNotResizableWebClient extends WebClient {
  override async notifyPanelWillOpen(_panelOpeningData: PanelOpeningData):
      Promise<void|OpenPanelInfo> {
    return {startingMode: WebClientMode.TEXT, canUserResize: false};
  }
}

class InitiallyNotResizableTest extends ApiTestFixtureBase {
  override createWebClient(): WebClient {
    return new InitiallyNotResizableWebClient();
  }

  async testInitiallyNotResizable() {
    await sleep(100);
  }
}

// All test fixtures. We look up tests by name, and the fixture name is ignored.
// Therefore all tests must have unique names.
const TEST_FIXTURES = [
  ApiTests,
  NotifyPanelWillOpenTest,
  InitiallyNotResizableTest,
  ApiTestWithoutOpen,
  ApiTestFailsToInitialize,
];

function findTestFixture(testName: string): any {
  for (const fixture of TEST_FIXTURES) {
    if (Object.getOwnPropertyNames(fixture.prototype).includes(testName)) {
      return fixture;
    }
  }
  return undefined;
}

// Result of running a test.
type TestResult =
    // The test completed successfully.
    'pass'|
    // A test step is complete. `continueApiTest()` needs to be called to
    // finish. The second value is the data passed to `nextStep()`.
    {id: 'next-step', payload: any}|
    // Any other string is an error.
    string;

// Runs a test.
class TestRunner implements TestStepper {
  nextStepPromise = Promise.withResolvers<{id: 'next-step', payload: any}>();
  continuePromise = Promise.withResolvers<void>();
  fixture: ApiTestFixtureBase|undefined;
  testDone: Promise<void>|undefined;
  testFound = false;
  constructor(private testName: string) {
    console.info(`TestRunner(${testName})`);
  }

  async setUp() {
    let fixtureCtor = findTestFixture(this.testName);
    if (!fixtureCtor) {
      // Note: throwing an exception here will not make it to the c++ side.
      // Wait until later to throw an error.
      console.error(`Test case not found: ${this.testName}`);
      this.testName = 'testDoNothing';
      fixtureCtor = findTestFixture(this.testName);
    } else {
      this.testFound = true;
    }
    this.fixture = (new fixtureCtor(this)) as ApiTestFixtureBase;
    return await this.fixture.setUpClient();
  }

  // Sets up the test and starts running it.
  async run(payload: any): Promise<TestResult> {
    assertTrue(this.testFound, `Test not found`);
    console.info(`Running test ${this.testName} with payload ${
        JSON.stringify(payload)}`);
    this.fixture!.testParams = payload;
    await this.fixture!.setUpTest();
    this.testDone = (this.fixture as any)[this.testName]() as Promise<void>;
    return this.continueTest();
  }

  // If `run()` or `stepComplete()` returns 'next-step', this function is called
  // to continue running the test.
  stepComplete(payload: any): Promise<TestResult> {
    console.info(`Continue test${this.testName}`);
    if (payload !== null) {
      this.fixture!.testParams = payload;
    }
    this.nextStepPromise = Promise.withResolvers();
    const continueResolve = this.continuePromise.resolve;
    this.continuePromise = Promise.withResolvers();
    continueResolve();
    return this.continueTest();
  }

  private async continueTest(): Promise<TestResult> {
    try {
      const result =
          await Promise.race([this.testDone, this.nextStepPromise.promise]);
      if (result && typeof result === 'object' &&
          result['id'] === 'next-step') {
        return result;
      }
    } catch (e) {
      if (e instanceof Error) {
        console.error(await improveStackTrace(e.stack ?? ''));
      }
      return `fail: ${e}`;
    }
    return 'pass';
  }

  // TestStepper implementation.
  nextStep(payload: any): Promise<void> {
    console.info(`Waiting for next step in test ${this.testName}`);
    payload = payload ?? {};  // undefined is not serializable to base::Value.
    this.nextStepPromise.resolve({id: 'next-step', payload});
    return this.continuePromise.promise;
  }
}

// Adds js source lines to the stack trace.
async function improveStackTrace(stack: string) {
  const outLines = [];
  for (const line of stack.split('\n')) {
    const m = line.match(/^\s+at.*\((.*):(\d+):(\d+)\)$/);
    if (m) {
      try {
        const [file, lineNo, column] = m.slice(1);
        const response = await fetch(file!);
        const text = await response.text();
        const lines = text.split('\n');
        const lineStr = lines[Number(lineNo) - 1];
        outLines.push(`${line.trim()}\n- ${lineStr}\n  ${
                                    ' '.repeat(Number(column) - 1)}^`);
      } catch (e) {
        outLines.push(`${line}`);
      }
    } else {
      outLines.push(line);
    }
  }
  return outLines.join('\n');
}

async function main() {
  console.info('api_test waiting for GlicHostRegistry');
  glicHostRegistry.resolve(await createGlicHostRegistryOnLoad());

  // If no test is selected, load a client that does nothing.
  // This is present because test.html is used as a dummy test client in
  // some tests.
  const testRunner = new TestRunner(getTestName() ?? 'testDoNothing');
  await testRunner.setUp();

  (window as any).runApiTest = (payload: any): Promise<TestResult> => {
    return testRunner.run(payload);
  };

  (window as any).continueApiTest = (payload: any): Promise<TestResult> => {
    return testRunner.stepComplete(payload);
  };
}


type ComparableValue = boolean|string|number|undefined|null;

function assertTrue(x: boolean, message?: string): asserts x {
  if (!x) {
    throw new Error(`assertTrue failed: '${x}' is not true. ${message ?? ''}`);
  }
}

function assertFalse(x: boolean, message?: string): asserts x is false {
  if (x) {
    throw new Error(
        `assertFalse failed: '${x}' is not false. ${message ?? ''}`);
  }
}

function assertEquals(
    a: ComparableValue, b: ComparableValue, message?: string) {
  if (a !== b) {
    throw new Error(`assertEquals('${a}', '${b}') failed. ${message ?? ''}`);
  }
}

function sleep(timeoutMs: number): Promise<void> {
  return new Promise((resolve) => {
    window.setTimeout(resolve, timeoutMs);
  });
}

main();

