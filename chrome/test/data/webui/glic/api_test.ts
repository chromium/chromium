// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Hot Tip! Generate a tsconfig.json file to get language server support. Run:
// ash/webui/personalization_app/tools/gen_tsconfig.py --root_out_dir out/pc \
//   --gn_target chrome/test/data/webui/glic:build_ts

import {ScrollToErrorReason, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {GlicBrowserHost, GlicHostRegistry, GlicWebClient, Observable, OpenPanelInfo, PanelOpeningData, ScrollToError, Subscriber} from '/glic/glic_api/glic_api.js';
import {ObservableValue} from '/glic/observable.js';

import {createGlicHostRegistryOnLoad} from './api_boot.js';

let maxTimeoutEndTime = performance.now() + 10000;

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
  isEmpty(): boolean {
    return this.readIndex === this.writeIndex;
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
  onNotifyPanelWasClosed: () => void = () => {};
  panelOpenState = ObservableValue.withValue<boolean>(false);

  async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    this.host = glicBrowserHost;
    this.initializedPromise.resolve();
  }

  async notifyPanelWillOpen(_panelOpeningData: PanelOpeningData):
      Promise<OpenPanelInfo> {
    this.panelOpenState.assignAndSignal(true);
    this.firstOpened.resolve();

    const openPanelInfo: OpenPanelInfo = {
      startingMode: WebClientMode.TEXT,
    };
    return openPanelInfo;
  }

  async notifyPanelWasClosed(): Promise<void> {
    this.onNotifyPanelWasClosed();
    this.panelOpenState.assignAndSignal(false);
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

  // Return to the C++ side, and wait for it to call ContinueJsTest() to
  // continue execution in the JS test. Optionally, pass data to the C++ side.
  advanceToNextStep(data?: any): Promise<void> {
    return this.testStepper.nextStep(data);
  }

  // Sets up the web client. This is called when the web contents loads,
  // before `ExecuteJsTest()`.
  async setUpClient() {
    this.setUpWithClient(this.createWebClient());
  }

  async setUpWithClient(client: WebClient) {
    const registry = await glicHostRegistry.promise;
    this.clientValue = client;
    await registry.registerWebClient(client);
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

// Test cases here correspond to test cases in glic_api_browsertest.cc.
// Since these tests run in the webview, this test can't use normal deps like
// mocha or chai assert.
class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  // WARNING: Remember to update
  // chrome/browser/glic/host/glic_api_browsertest.cc if you add a new test!

  async testDoNothing() {}

  async testAllTestsAreRegistered() {
    const allNames = [];
    for (const fixture of TEST_FIXTURES) {
      allNames.push(...Object.getOwnPropertyNames(fixture.prototype)
                        .filter(name => name.startsWith('test')));
    }
    await this.advanceToNextStep(allNames);
  }

  async testRequestHeader() {}

  async testCreateTab() {
    assertTrue(!!this.host.createTab);
    // Open a tab pointing to test.html.
    const url = location.href;
    const data = await this.host.createTab(url, {openInBackground: false});
    assertEquals(data.url, url);
  }

  async testCreateTabFailsWithUnsupportedScheme() {
    assertTrue(!!this.host.createTab);

    this.assertCreateTabFails('chrome://settings');
    this.assertCreateTabFails('ftps://www.google.com');
    this.assertCreateTabFails('chrome-extension://www.google.com');
    this.assertCreateTabFails('mailto:user@google.com');
    this.assertCreateTabFails(
        'data:text/html;charset=utf-8,<html>Hello World</html>');
    this.assertCreateTabFails('file:///tmp/test.html');
  }

  async testCreateTabInBackground() {
    assertTrue(!!this.host.createTab);

    await this.host.createTab(
        location.href + '#foreground', {openInBackground: false});

    await this.advanceToNextStep();

    await this.host.createTab(
        location.href + '#background', {openInBackground: true});
  }

  async testCreateTabByClickingOnLink() {
    assertTrue(!!this.host.setAudioDucking);
    // Check that audio ducking still works after clicking a link.
    this.host.setAudioDucking(true);
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();

    await this.advanceToNextStep();
    this.host.setAudioDucking(false);
  }

  async testCreateTabFailsIfNotActive() {
    assertTrue(!!this.host.closePanel);
    assertTrue(!!this.host.createTab);
    await this.closePanelAndWaitUntilInactive();
    this.assertCreateTabFails(location.href);
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

    // Close the panel, and verify notifyPanelWasClosed is called.
    const closedPromise = Promise.withResolvers<void>();
    this.client.onNotifyPanelWasClosed = closedPromise.resolve;
    await this.host.closePanel();
    await waitFor(closedPromise.promise);
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

  async testGetZeroStateSuggestions() {
    assertTrue(!!this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertTrue(!!suggestions);
    assertEquals(0, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsFailsWhenHidden() {
    assertTrue(!!this.host.getZeroStateSuggestionsForFocusedTab);
    assertTrue(!!this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertTrue(!!suggestions);
    assertEquals(0, suggestions.suggestions.length);
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

  async testGetFocusedTabStateV2WithNavigation() {
    // Initial state.
    assertTrue(!!this.host.getFocusedTabStateV2);
    const sequence = observeSequence(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertTrue(!!focus.hasFocus);
    assertTrue(
        focus.hasFocus.tabData.url.endsWith('glic/test.html'),
        `url=${focus.hasFocus.tabData.url}`);
    assertFalse(!!focus.hasNoFocus);

    // After a second navigation occurs.
    await this.advanceToNextStep();
    const focus2 = await sequence.next();
    assertTrue(!!focus2.hasFocus);
    assertTrue(
        focus2.hasFocus.tabData.url.endsWith(
            'scrollable_page_with_content.html'),
        `url=${focus2.hasFocus.tabData.url}`);

    await this.advanceToNextStep();
    let focus3 = await sequence.next();

    // After a navigation occurs in a new tab, there could first exist a
    // transitory states where the focus is not yet available, is empty, or
    // still previous page.
    while (focus3.hasNoFocus ||
           (!!focus3.hasFocus &&
            (focus3.hasFocus.tabData.url === '' ||
             focus3.hasFocus.tabData.url.endsWith(
                 'scrollable_page_with_content.html')))) {
      focus3 = await sequence.next();
    }

    // Final state, after the tab is fully loaded.
    assertTrue(!!focus3.hasFocus);
    assertTrue(
        focus3.hasFocus.tabData.url.endsWith('glic/test.html'),
        `url=${focus3.hasFocus.tabData.url}`);
    assertFalse(!!focus3.hasNoFocus);
  }

  async testGetFocusedTabStateV2WithNavigationWhenInactive() {
    // Initial state.
    assertTrue(!!this.host.getFocusedTabStateV2);
    await this.closePanelAndWaitUntilInactive();
    const sequence = observeSequence(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertTrue(!!focus.hasFocus);
    assertTrue(
        focus.hasFocus.tabData.url.endsWith('glic/test.html'),
        `url=${focus.hasFocus.tabData.url}`);
    assertFalse(!!focus.hasNoFocus);

    await this.closePanelAndWaitUntilInactive();

    // After we hide, two navigations will occur. The second in a new tab.
    await this.advanceToNextStep();

    const focus2 = await runUntil(async () => {
      const nextFocus = await sequence.next();

      // After a navigation occurs in a new tab, there could first exist a
      // transitory states where the focus is not yet available, is empty, or
      // still previous page.
      if (!nextFocus || !!nextFocus.hasNoFocus || !nextFocus.hasFocus) {
        return undefined;
      }

      const focused_url = nextFocus.hasFocus.tabData.url;
      if (focused_url === '' ||
          focused_url.endsWith('scrollable_page_with_content.html')) {
        return undefined;
      }
      return nextFocus;
    });

    // Final state, after the tab is fully loaded.
    assertFalse(
        !!focus2.hasFocus &&
        focus2.hasFocus.tabData.url.endsWith(
            'scrollable_page_with_content.html'));
    assertTrue(!!focus2.hasFocus);
    assertTrue(
        focus2.hasFocus.tabData.url.endsWith('glic/test.html'),
        `url=${focus2.hasFocus.tabData.url}`);
    assertFalse(!!focus2.hasNoFocus);
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

  async testGetContextFromFocusedTabWithPdfFile() {
    await this.host.setTabContextPermissionState(true);

    // Pdf pages have two loads: one of the WebContents, and another of the
    // element within an iframe that contains the actual pdf. We need to wait
    // for both to be finished before running the test. The cpp side waits for
    // the WebContents to be loaded, but we must still wait here.
    const result = await runUntil(async () => {
      const result =
          await this.host.getContextFromFocusedTab?.({pdfData: true});
      if (!result || !result.pdfDocumentData ||
          !result.pdfDocumentData.pdfData) {
        return undefined;
      }
      return result;
    });

    assertTrue(
        result.tabData.url.endsWith('pdf/test.pdf') ?? false,
        `Tab data has unexpected url ${result.tabData.url}`);
    assertFalse(!!result.webPageData);

    // Original PDF size is 7984 bytes, because Chrome reserializes the PDF,
    // the size can change, but it shouldn't be too small.
    const pdfData = await readStream(result.pdfDocumentData!.pdfData!);
    assertTrue(
        pdfData.byteLength > 5000,
        `PDF data is too short. length=${pdfData.byteLength}`);
    assertEquals('%PDF', new TextDecoder().decode(pdfData.slice(0, 4)));
    assertFalse(result.pdfDocumentData!.pdfSizeLimitExceeded);
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
    let hotkeyState = await osHotkeyState.next();
    const isMac = /Mac/.test(navigator.platform);
    let expectedHotkey = isMac ? '<⌃>-<G>' : '<Ctrl>-<G>';
    assertEquals(expectedHotkey, hotkeyState.hotkey);
    await this.advanceToNextStep();
    hotkeyState = await osHotkeyState.next();
    expectedHotkey = isMac ? '<⌃>-<⇧>-<1>' : '<Ctrl>-<Shift>-<1>';
    assertEquals(expectedHotkey, hotkeyState.hotkey);
    await this.advanceToNextStep();
    hotkeyState = await osHotkeyState.next();
    expectedHotkey = '';
    assertEquals(expectedHotkey, hotkeyState.hotkey);
  }

  async testClosedCaptioning() {
    assertTrue(!!this.host.getClosedCaptioningSetting);
    assertTrue(!!this.host.setClosedCaptioningSetting);
    const closedCaptioningState =
        observeSequence(this.host.getClosedCaptioningSetting());
    assertFalse(await closedCaptioningState.next());
    await this.host.setClosedCaptioningSetting(true);
    assertTrue(await closedCaptioningState.next());
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

  async testGetUserProfileInfoDefersWhenInactive() {
    assertTrue(!!this.host.getUserProfileInfo);
    assertTrue(!!this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    const promise = this.host.getUserProfileInfo();
    try {
      await waitFor(promise, 200);
      // We should have thrown here as the promise should not resolve until
      // advancing to the next step.
      assertTrue(false);
    } catch {
    }
    await this.advanceToNextStep();
    const profileInfo = await promise;
    assertEquals('glic-test@example.com', profileInfo.email);
  }

  async testRefreshSignInCookies() {
    assertTrue(!!this.host.refreshSignInCookies);

    await this.host.refreshSignInCookies();
  }

  async testSignInPauseState() {
    assertTrue(!!this.host.getUserProfileInfo);
    const profileInfo = await this.host.getUserProfileInfo();

    assertEquals('', profileInfo.displayName);
    assertEquals('glic-test@example.com', profileInfo.email);
    assertEquals('', profileInfo.givenName);
    assertEquals(false, profileInfo.isManaged!);
    assertTrue((profileInfo.localProfileName?.length ?? 0) > 0);
  }

  async testSetContextAccessIndicator() {
    assertTrue(!!this.host.setContextAccessIndicator);

    await this.host.setContextAccessIndicator(true);
  }

  async testSetAudioDucking() {
    assertTrue(!!this.host.setAudioDucking);

    await this.host.setAudioDucking(true);
  }

  async testGetDisplayMedia() {
    async function waitForFirstFrame(track: MediaStreamVideoTrack):
        Promise<boolean> {
      const processor = new MediaStreamTrackProcessor({track});
      const reader = processor.readable.getReader();

      try {
        const result = await reader.read();
        if (result.done) {
          throw new Error('Track ended before a frame could be read.');
        }
        const frame = result.value;  // This is a VideoFrame
        frame.close();
        return true;
      } finally {
        reader.releaseLock();
      }
    }

    // The client should be able to use getDisplayMedia() to capture the glic
    // window.
    const stream = await navigator.mediaDevices.getDisplayMedia({
      video: true,
      audio: false,
      preferCurrentTab: true,
    } as any);
    const videoTracks = stream.getVideoTracks();
    assertTrue(videoTracks.length > 0);
    const track = videoTracks[0] as MediaStreamVideoTrack;
    assertTrue(!!track);
    assertTrue(await waitForFirstFrame(track));
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
    assertTrue(!!this.host.setTabContextPermissionState);
    await this.host.setTabContextPermissionState(true);
    await this.host.scrollTo(
        {selector: {exactText: {text: 'Test Page'}}, highlight: true});
  }

  async testScrollToFindsTextNoTabContextPermission() {
    assertTrue(!!this.host.scrollTo);
    try {
      await this.host.scrollTo(
          {selector: {exactText: {text: 'Abracadabra'}}, highlight: true});
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.TAB_CONTEXT_PERMISSION_DISABLED,
          (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  async testScrollToFailsWhenInactive() {
    assertTrue(!!this.host.scrollTo);
    assertTrue(!!this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    try {
      await this.host.scrollTo(
          {selector: {exactText: {text: 'Abracadabra'}}, highlight: true});
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.NOT_SUPPORTED, (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  async testScrollToNoMatchFound() {
    assertTrue(!!this.host.scrollTo);
    assertTrue(!!this.host.setTabContextPermissionState);
    await this.host.setTabContextPermissionState(true);
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

  async testManualResizeChanged() {
    assertTrue(!!this.host.isManuallyResizing);
    await observeSequence(this.host.isManuallyResizing()).waitForValue(true);

    await this.advanceToNextStep();
    await observeSequence(this.host.isManuallyResizing()).waitForValue(false);
  }

  async testResizeWindowTooSmall() {
    assertTrue(!!this.host.resizeWindow);
    await this.host.resizeWindow(0, 0);
  }

  async testResizeWindowTooLarge() {
    assertTrue(!!this.host.resizeWindow);
    await this.host.resizeWindow(20000, 20000);
  }

  async testResizeWindowWithinBounds() {
    assertTrue(!!this.host.resizeWindow);
    assertTrue(!!this.testParams);
    await this.host.resizeWindow(this.testParams.width, this.testParams.height);
  }

  async testGetContextFromFocusedTabWithIneligiblePage() {
    assertTrue(!!this.host.getContextFromFocusedTab);
    await this.host.setTabContextPermissionState(true);

    try {
      await this.host.getContextFromFocusedTab?.({
        innerText: true,
        viewportScreenshot: true,
        annotatedPageContent: true,
        maxMetaTags: 32,
        pdfData: true,
      });
    } catch (e) {
      assertEquals(
          'tabContext failed: page context ineligible', (e as Error).message);
      return;
    }
    assertTrue(false, 'getContextFromFocusedTab should have thrown an error');
  }

  async testGetContextFromFocusedTabWithEligiblePage() {
    await this.host.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab?.({
      innerText: true,
      viewportScreenshot: true,
      annotatedPageContent: true,
      maxMetaTags: 32,
      pdfData: true,
    });

    assertTrue(!!result);
  }

  async testOpenOsMediaPermissionSettings() {
    assertTrue(!!this.host.openOsPermissionSettingsMenu);
    this.host.openOsPermissionSettingsMenu('media');
  }

  async testOpenOsGeoPermissionSettings() {
    assertTrue(!!this.host.openOsPermissionSettingsMenu);
    this.host.openOsPermissionSettingsMenu('geolocation');
  }

  async testGetOsMicrophonePermissionStatusAllowed() {
    assertTrue(!!this.host.getOsMicrophonePermissionStatus);
    assertTrue(await this.host.getOsMicrophonePermissionStatus());
  }

  async testGetOsMicrophonePermissionStatusNotAllowed() {
    assertTrue(!!this.host.getOsMicrophonePermissionStatus);
    assertFalse(await this.host.getOsMicrophonePermissionStatus());
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

  async testCallingApiWhileHiddenRecordsMetrics() {
    assertTrue(!!this.host.createTab);
    await this.advanceToNextStep();
    await runUntil(() => document.visibilityState === 'hidden');
    try {
      await this.host.createTab(
          'https://www.google.com', {openInBackground: false});
    } catch {
    }
  }

  async testReloadWebUi() {}

  private async assertCreateTabFails(url: string) {
    assertTrue(!!this.host.createTab);
    const errorMessage = await assertRejects(
        this.host.createTab(url, {openInBackground: false}));
    assertEquals('createTab: failed', errorMessage);
  }

  private async closePanelAndWaitUntilInactive() {
    assertTrue(!!this.host.closePanel);
    await this.host.closePanel();
    await observeSequence(this.host.panelActive()).waitForValue(false);
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

  async testDeferredFocusedTabStateAtCreation() {
    // Initial state.
    assertTrue(!!this.host.getFocusedTabStateV2);
    const focusedTabStateV2Sequence =
        observeSequence(this.host.getFocusedTabStateV2());
    let focusedTabState = await focusedTabStateV2Sequence.next();
    assertTrue(!!focusedTabState.hasNoFocus);
    const tabStatePromise = focusedTabStateV2Sequence.next();
    assertRejects(waitFor(tabStatePromise, 200));
    // We should only see the second page.
    await this.advanceToNextStep();
    focusedTabState = await tabStatePromise;
    assertTrue(!!focusedTabState.hasFocus);
    assertTrue(
        focusedTabState.hasFocus.tabData.url.endsWith(
            'scrollable_page_with_content.html'),
        `url=${focusedTabState.hasFocus.tabData.url}`);
  }
}

type InitFailureType = 'error'|'timeout'|'none'|'reloadAfterInitialize'|
    'navigateToSorryPageBeforeInitialize'|'navigateToSorryPageAfterInitialize';

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
    if (this.failWith === 'navigateToSorryPageBeforeInitialize') {
      location.href = '/sorry/index.html';
      return sleep(5000);
    }
    if (this.failWith === 'navigateToSorryPageAfterInitialize') {
      sleep(500).then(() => {
        location.href = '/sorry/index.html';
      });
    }
    // This initialization is sometimes skipped depending on the type of desired
    // failure detected above
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

  // Runs ApiTestFixtureBase.setUpClient() after 100 ms, and returns
  // immediately. This allows the test to exit cleanly before the web contents
  // is torn down.
  deferredSetUpClient() {
    sleep(100).then(() => super.setUpClient());
  }

  async testInitializeFailsWindowClosed() {
    this.deferredSetUpClient();
  }

  async testInitializeFailsWindowOpen() {
    this.deferredSetUpClient();
  }

  async testReload() {
    // First run.
    if (this.getTestParams().failWith === 'reloadAfterInitialize') {
      this.deferredSetUpClient();
      return;
    }

    // Second run. Client should initialize and be opened.
    await super.setUpClient();
    await this.client.waitForFirstOpen();
  }

  async testSorryPageBeforeInitialize() {
    this.deferredSetUpClient();
  }

  async testSorryPageAfterInitialize() {
    this.deferredSetUpClient();
  }

  async testInitializeFailsAfterReload() {
    this.deferredSetUpClient();
  }
  // Skips the setup entirely.
  async testNoClientCreated() {}
  // Skips the bootstrap as well. The test name "testNoBootstrap" is handled
  // specially.
  async testNoBootstrap() {}
  async testInitializeTimesOut() {
    await super.setUpClient();
  }

  // Tests notifyPanelWillOpen() returning after the panel is closed and then
  // reopened.
  async testCloseAndOpenWhileOpening() {
    const openSignal = Promise.withResolvers<void>();
    class WebClientThatOpensSlowly extends WebClient {
      override async notifyPanelWillOpen(): Promise<OpenPanelInfo> {
        this.panelOpenState.assignAndSignal(true);
        await openSignal.promise;
        return {
          startingMode: WebClientMode.TEXT,
        };
      }
    }
    await this.setUpWithClient(new WebClientThatOpensSlowly());
    const panelOpenState = observeSequence(this.client.panelOpenState);
    panelOpenState.waitForValue(true);
    await this.host.closePanel!();
    panelOpenState.waitForValue(false);
    await this.advanceToNextStep();
    openSignal.resolve();
    await panelOpenState.waitForValue(true);
  }
}

class WebClientThatOpensOnce extends WebClient {
  notifyPanelWillOpenCallCount = 0;
  override async notifyPanelWillOpen(panelOpeningData: PanelOpeningData):
      Promise<OpenPanelInfo> {
    this.notifyPanelWillOpenCallCount += 1;
    return super.notifyPanelWillOpen(panelOpeningData);
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
      Promise<OpenPanelInfo> {
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
  async run(maxTimeoutMs: number, payload: any): Promise<TestResult> {
    assertTrue(this.testFound, `Test not found`);
    maxTimeoutEndTime = performance.now() + maxTimeoutMs;
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
  if (getTestName() !== 'testNoBootstrap') {
    console.info('api_test waiting for GlicHostRegistry');
    glicHostRegistry.resolve(await createGlicHostRegistryOnLoad());
  }

  // If no test is selected, load a client that does nothing.
  // This is present because test.html is used as a dummy test client in
  // some tests.
  const testRunner = new TestRunner(getTestName() ?? 'testDoNothing');
  await testRunner.setUp();

  (window as any).runApiTest =
      (maxTimeoutMs: number, payload: any): Promise<TestResult> => {
        return testRunner.run(maxTimeoutMs, payload);
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

function getTimeout(timeoutMs?: number): number {
  if (timeoutMs === undefined) {
    return Math.max(0, maxTimeoutEndTime - performance.now());
  }
  return timeoutMs;
}

// Waits for a promise to resolve. If the timeout is reached first, throws an
// exception. Note this is useful because if the test times out in the normal
// way, we do not receive a very useful error.
async function waitFor<T>(value: Promise<T>, timeoutMs?: number): Promise<T> {
  const timeoutResult = Symbol();
  const result = await Promise.race(
      [value, sleep(getTimeout(timeoutMs)).then(() => timeoutResult)]);
  if (result === timeoutResult) {
    throw new Error(`Timed out while waiting`);
  }
  return value;
}


// Run until `condition()` returns a truthy value. Throws an exception if the
// timeout is reached first. Otherwise, this returns the value returned by
// condition.
async function runUntil<T>(
    condition: () => T | PromiseLike<T>,
    timeoutMs?: number): Promise<NonNullable<T>> {
  timeoutMs = getTimeout(timeoutMs);
  const sleepMs = getTimeout(timeoutMs) / 20;
  const timeout = performance.now() + timeoutMs;
  while (performance.now() < timeout) {
    const result = await condition();
    if (result) {
      return result;
    }
    await sleep(sleepMs);
  }
  throw new Error('runUntil timed out');
}

function readStream(stream: ReadableStream<Uint8Array>): Promise<Uint8Array> {
  return new Response(stream).bytes();
}

async function assertRejects<T>(promise: Promise<T>):
    Promise<string|undefined> {
  return promise.then(
      () => {
        // The promise should have rejected.
        assertTrue(false);
      },
      (e) => {
        return (e as Error).message;
      });
}

main();
