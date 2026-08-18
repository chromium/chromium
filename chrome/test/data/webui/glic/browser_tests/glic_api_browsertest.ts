// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {HostCapability, MetricUserInputReactionType, PanelStateKind, Platform, ResponseStopCause, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {FocusedTabData, TabData, UserProfileInfo} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertNotEquals, assertRejects, assertTrue, assertUndefined, checkDefined, mapObservable, observeSequence, readStream, runUntil, sleep, testMain} from './browser_test_base.js';
import type {SequencedSubscriber} from './browser_test_base.js';

// Test cases here correspond to test cases in glic_api_browsertest.cc.
// Since these tests run in the webview, this test can't use normal deps like
// mocha or chai assert.
class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async detachIfInMultiInstance() {
    if (this.isMultiInstanceEnabled()) {
      assertDefined(this.host.detachPanel);
      this.host.detachPanel();

      assertDefined(this.host.getPanelState);
      const panelStates = observeSequence(this.host.getPanelState());
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.DETACHED);
    }
  }

  isMultiInstanceEnabled(): boolean {
    return !!this.host.getHostCapabilities?.()?.has(
        HostCapability.MULTI_INSTANCE);
  }


  // WARNING: Remember to update
  // chrome/browser/glic/host/glic_api_browsertest.cc if you add a new test!

  async testHibernateAllOnMemoryPressure() {}






  async testErrorShownOnMojoPipeError() {}

  async testPanelActiveWithMicrophone() {
    await this.advanceToNextStep();
    await this.advanceToNextStep();
  }

  async testGetContextFromFocusedTabWithoutPermission() {
    assertDefined(this.host.getContextFromFocusedTab);
    await this.host.setTabContextPermissionState(false);

    await assertRejects(this.host.getContextFromFocusedTab({}), {
      withErrorMessage: 'tabContext failed: permission denied:' +
          ' context permission not enabled',
    });
  }

  async testGetContextFromPinnedTabWithoutPermission() {
    assertDefined(this.host.getContextFromTab);
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.pinTabs);
    await this.host.setTabContextPermissionState(false);

    const focusSequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await focusSequence.next();
    const tabId = checkDefined(focus?.hasFocus?.tabData.tabId);

    // Tab is already pinned in multi-instance mode.
    if (!this.isMultiInstanceEnabled()) {
      assertTrue(await this.host.pinTabs([tabId]));
    }

    const result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(
        new URL(result.tabData.url).pathname, '/glic/browser_tests/test.html',
        `Tab data has unexpected url ${result.tabData.url}`);
  }

  async testGetContextFromFocusedTabWithNoRequestedData() {
    assertDefined(this.host.getContextFromFocusedTab);
    await this.host.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab({});
    assertDefined(result);
    assertEquals(
        new URL(result.tabData.url).pathname, '/glic/browser_tests/test.html',
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

    assertDefined(result);

    assertEquals(
        new URL(result.tabData.url).pathname, '/glic/browser_tests/test.html',
        `Tab data has unexpected url ${result.tabData.url}`);
    assertFalse(!!result.pdfDocumentData);  // The page is not a PDF.
    assertDefined(result.webPageData);
    assertEquals(
        'This is a test page', result.webPageData.mainDocument.innerText);
    assertDefined(result.viewportScreenshot);
    assertTrue(
        (result.viewportScreenshot.data.byteLength ?? 0) > 0,
        `Expected viewport screenshot bytes, got ${
            result.viewportScreenshot.data.byteLength}`);
    assertTrue(result.viewportScreenshot.heightPixels > 0);
    assertTrue(result.viewportScreenshot.widthPixels > 0);
    assertEquals('image/jpeg', result.viewportScreenshot.mimeType);
    assertDefined(result.annotatedPageData);
    const annotatedPageContentSize =
        (await new Response(result.annotatedPageData.annotatedPageContent)
             .bytes())
            .length;
    assertTrue(annotatedPageContentSize > 1);

    // Check metadata.
    assertDefined(result.annotatedPageData.metadata);
    assertDefined(result.annotatedPageData.metadata.frameMetadata);
    assertEquals(result.annotatedPageData.metadata.frameMetadata.length, 1);
    const frameMetadata = result.annotatedPageData.metadata.frameMetadata[0];
    assertDefined(frameMetadata);
    const url: URL = new URL(frameMetadata.url);
    assertEquals(url.pathname, '/glic/browser_tests/test.html');
    assertEquals(frameMetadata.metaTags.length, 1);
    const metaTag = frameMetadata.metaTags[0];
    assertDefined(metaTag);
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

    assertEquals(
        new URL(result.tabData.url).pathname, '/pdf/test.pdf',
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

  async testGetContextFromFocusedTabWithUnFocusablePage() {
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.getContextFromFocusedTab);
    assertDefined(this.host.setTabContextPermissionState);

    // Confirms that the current tab has an un-focusable page.
    const focusSequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await focusSequence.next();
    assertDefined(focus.hasNoFocus);
    assertTrue(focusSequence.isEmpty());

    // Focused tab extraction should fail for an un-focusable page.
    await this.host.setTabContextPermissionState(true);
    await assertRejects(this.host.getContextFromFocusedTab({}), {
      withErrorMessage: 'tabContext failed: permission denied',
    });
  }

  async testGetContextForActorFromTabWithoutPermission() {
    await this.host.setTabContextPermissionState(true);
    assertDefined(this.host.getFocusedTabStateV2);
    const focusedTab = await this.host.getFocusedTabStateV2().getCurrentValue();
    assertDefined(focusedTab?.hasFocus?.tabData?.tabId);
    await this.host.setTabContextPermissionState(false);
    const result = await this.host.getContextForActorFromTab?.(
        focusedTab.hasFocus.tabData.tabId, {});
    assertDefined(result);
  }

  async testGetContextForActorFromTabWithRestrictedUrl() {
    await this.host.setTabContextPermissionState(true);
    assertDefined(this.host.getFocusedTabStateV2);
    const focusedTab = await this.host.getFocusedTabStateV2().getCurrentValue();
    assertDefined(focusedTab?.hasNoFocus?.tabFocusCandidateData?.tabId);
    const tabId = focusedTab.hasNoFocus.tabFocusCandidateData.tabId;
    await assertRejects(this.host.getContextForActorFromTab!(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied',
    });
  }

  // TODO(crbug.com/422544382): add test for getContextForActorFromTab for the
  // case where tab is in background.

  // TODO(harringtond): This is disabled because it hangs. Fix it.
  async testCaptureScreenshot() {
    assertDefined(this.host.captureScreenshot);
    const screenshot = await this.host.captureScreenshot?.();
    assertDefined(screenshot);
    assertTrue(screenshot.widthPixels > 0);
    assertTrue(screenshot.heightPixels > 0);
    assertTrue(screenshot.data.byteLength > 0);
    assertEquals(screenshot.mimeType, 'image/jpeg');
  }



  async testGetUserProfileInfoCached() {
    assertDefined(this.host.getUserProfileInfo);
    assertDefined(this.host.getPlatform);

    // 1. Fetch the profile (non-cached).
    const profileInfo1 = await this.host.getUserProfileInfo();

    // Verify basic data validity.
    assertEquals('Glic Testing', profileInfo1.displayName);
    assertEquals('glic-test@example.com', profileInfo1.email);

    // 2. Fetch the profile again (cached).
    const profileInfo2 = await this.host.getUserProfileInfo();

    // 3. Verify that the returned object is the *same instance* as the first
    // one.
    assertTrue(
        profileInfo1 === profileInfo2,
        'Expected cached profile object identity to match');

    // 4. Verify Avatar Blob Caching (Lazy Loading).
    if (profileInfo1.avatarIcon) {
      const avatarPromise1 = profileInfo1.avatarIcon();
      const avatarPromise2 = profileInfo1.avatarIcon();

      // Ensure that the implementation caches the promise itself.
      assertTrue(
          avatarPromise1 === avatarPromise2,
          'Expected avatar promise identity to match');

      const blob1 = await avatarPromise1;

      // If the user has an avatar, verify the blob.
      if (blob1) {
        assertTrue(blob1.size > 0);
      }
    }
  }

  async testGetUserProfileInfoDoesNotDeferWhenInactive() {
    assertDefined(this.host.getUserProfileInfo);
    assertDefined(this.host.closePanel);
    assertDefined(this.host.getPlatform);
    await this.closePanelAndWaitUntilInactive();
    const profileInfo: UserProfileInfo = await this.host.getUserProfileInfo();
    const platform = await this.host.getPlatform();
    assertEquals('glic-test@example.com', profileInfo.email);
    if (platform !== Platform.CHROME_OS) {
      // Can be 'Your Chrome' or 'Your Chromium'.
      assertEquals('Your C', profileInfo.localProfileName?.substring(0, 6));
    }
  }

  async testMetrics() {
    assertDefined(this.host.getMetrics);
    const metrics = this.host.getMetrics();
    assertDefined(metrics);
    assertDefined(metrics.onResponseRated);
    assertDefined(metrics.onUserInputSubmitted);
    assertDefined(metrics.onReaction);
    assertDefined(metrics.onContextUploadStarted);
    assertDefined(metrics.onContextUploadCompleted);
    assertDefined(metrics.onResponseStarted);
    assertDefined(metrics.onResponseStopped);
    assertDefined(metrics.onSessionTerminated);
    assertDefined(metrics.onClosedCaptionsShown);
    metrics.onResponseRated(true);
    metrics.onUserInputSubmitted(WebClientMode.TEXT);
    metrics.onContextUploadStarted();
    metrics.onContextUploadCompleted();
    metrics.onReaction(MetricUserInputReactionType.MODEL);
    metrics.onResponseStarted();
    metrics.onResponseStopped({cause: ResponseStopCause.USER});
    metrics.onSessionTerminated();
    metrics.onClosedCaptionsShown();
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
    assertEquals(url.pathname, '/glic/browser_tests/test.html');
  }

  async testNavigateToAboutBlank() {
    // Navigation to about:blank will destroy this test client, so the code
    // below will first allow this test function to return, and then navigate.
    (async () => {
      await sleep(100);
      location.href = 'about:blank';
    })();
  }


  // Helper function to pin the active tab. Asserts the tab is pinned, and
  // returns the tab ID.
  async pinActiveTab(): Promise<string> {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);
    const tabId = this.getActiveTabId();
    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.some(t => t.tabId === tabId));
    return tabId;
  }

  async testTabDataUpdateOnUrlChangeForPinnedTab() {
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.pinTabs);

    const tabId = this.testParams.tabId;
    assertNotEquals(tabId, this.getActiveTabId());

    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.some(t => t.tabId === tabId));

    // Navigate to a different URL.
    await this.advanceToNextStep();

    // Make sure that the pinned tab is not focused.
    assertNotEquals(tabId, this.getActiveTabId());
    await pinnedTabsUpdates.waitFor(
        (tabs) =>
            tabs.some(t => t.tabId === tabId && t.url.includes('changed')));
  }

  async testTabDataUpdateOnFaviconChangeForPinnedTab() {
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.pinTabs);

    const tabId = this.testParams.tabId;
    assertNotEquals(tabId, this.getActiveTabId());

    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());

    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 &&
            tabs.some(t => t.tabId === tabId && t.favicon === undefined));

    // Update the favicon.
    await this.advanceToNextStep();

    const [tabData] = await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 &&
            tabs.some(t => t.tabId === tabId && t.favicon !== undefined));

    const blob = await tabData?.favicon?.();
    assertEquals(blob?.type, 'image/png');
  }


  // Helper to get focused tabId.
  getFocusedTabId(): string {
    assertDefined(this.host.getFocusedTabStateV2);
    const focus = this.host.getFocusedTabStateV2().getCurrentValue();
    return checkDefined(focus?.hasFocus?.tabData.tabId);
  }

  // Asserts that there is an active tab, and returns its tab ID.
  getActiveTabId(): string {
    assertDefined(this.host.getFocusedTabStateV2);
    const focus = this.host.getFocusedTabStateV2().getCurrentValue();
    assertDefined(focus);
    // In multi-instance, the active tab isn't necessarily focused.
    if (!this.isMultiInstanceEnabled()) {
      assertDefined(focus.hasFocus);
    }
    if (focus.hasFocus) {
      return focus.hasFocus.tabData.tabId;
    }
    return checkDefined(focus.hasNoFocus?.tabFocusCandidateData?.tabId);
  }

  observeActiveTab(): SequencedSubscriber<TabData|undefined> {
    assertDefined(this.host.getFocusedTabStateV2);
    return observeSequence(
        mapObservable(this.host.getFocusedTabStateV2(), (focus) => {
          let active = focus?.hasFocus?.tabData;
          if (!active && this.isMultiInstanceEnabled()) {
            active = focus?.hasNoFocus?.tabFocusCandidateData;
          }
          return active;
        }));
  }


  // Helper for `testFetchInactiveTabScreenshot` and
  // `testFetchInactiveTabScreenshotWhileMinimized`.
  async fetchInactiveTabScreenshot(expectNoFocus: boolean = false) {
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.getContextFromTab);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);

    // Pin the focused tab.
    const focusSequence = observeSequence(this.host.getFocusedTabStateV2());
    let focus = await focusSequence.next();
    const tabId = checkDefined(focus?.hasFocus?.tabData.tabId);
    await this.host.pinTabs([tabId]);

    // Select the other tab.
    await this.advanceToNextStep();
    focus = await focusSequence.waitFor(
        (f) => (!!f.hasFocus && f.hasFocus.tabData.tabId !== tabId) ||
            (expectNoFocus && !!f.hasNoFocus));

    // Get context and verify we have a screenshot.
    const context = await this.host.getContextFromTab(tabId, {
      viewportScreenshot: true,
    });
    return context;
  }

  async testFetchInactiveTabScreenshot() {
    const context = await this.fetchInactiveTabScreenshot();
    assertFalse(checkDefined(context.tabData.isObservable));
    const screenshot = checkDefined(context.viewportScreenshot);
    assertEquals(screenshot.mimeType, 'image/jpeg');
    assertTrue(screenshot.data.byteLength > 0);
    assertTrue(screenshot.widthPixels > 0);
    assertTrue(screenshot.heightPixels > 0);
  }

  async testFetchInactiveTabScreenshotWhileMinimized() {
    const shouldGetScreenshot = this.testParams;
    // Tests fetching the screenshot of a tab while the browser is minimized.
    // Ideally this would work, but it currently times out and provides no
    // screenshot on some platforms.
    const context = await this.fetchInactiveTabScreenshot(
        /*expectNoFocus=*/ true);
    assertFalse(checkDefined(context.tabData.isObservable));

    if (shouldGetScreenshot) {
      assertDefined(context.viewportScreenshot);
    } else {
      // For platforms where screenshotting fails while minimized, it fails
      // randomly, so we don't assert anything here. This test at least confirms
      // screenshotting does not hang forever.
      // Note: I've tried adding a sleep between minimizing the window and
      // capturing the screenshot, but it still succeeds randomly.
    }
  }

  async testSwitchConversationToLastActiveConversation() {
    assertDefined(this.host.registerConversation);
    assertDefined(this.host.switchConversation);
    if (this.testParams === 'step1') {
      await this.host.registerConversation(
          {conversationId: 'A', conversationTitle: 'Title A'});
      await this.advanceToNextStep();
    } else if (this.testParams === 'step2') {
      // Return and then switch conversation to ensure that ExecuteJsTest
      // completes before the instance is deleted. The instance is deleted
      // during the `switchConversation` call.
      sleep(100).then(() => {
        assertDefined(this.host.switchConversation);
        this.host.switchConversation(
            {conversationId: 'A', conversationTitle: 'Title A'});
      });
    }
  }

  async testSwitchConversationToOldConversationInOldInstance() {
    assertDefined(this.host.registerConversation);
    assertDefined(this.host.switchConversation);
    if (this.testParams === 'step1') {
      await this.host.registerConversation(
          {conversationId: 'A', conversationTitle: 'Title A'});
      await this.advanceToNextStep();
    } else if (this.testParams === 'step2') {
      sleep(100).then(() => {
        assertDefined(this.host.switchConversation);
        this.host.switchConversation(
            {conversationId: 'B', conversationTitle: 'Title B'});
      });
    } else if (this.testParams === 'step3') {
      // Return and then switch conversation to ensure that ExecuteJsTest
      // completes before the instance is deleted. The instance is deleted
      // during the `switchConversation` call.
      sleep(100).then(() => {
        assertDefined(this.host.switchConversation);
        this.host.switchConversation(
            {conversationId: 'A', conversationTitle: 'Title A'});
      });
    }
  }

  async testMaybeRefreshUserStatus() {
    assertDefined(this.host.maybeRefreshUserStatus);
    this.host.maybeRefreshUserStatus();
  }

  async testMaybeRefreshUserStatusThrottled() {
    assertDefined(this.host.maybeRefreshUserStatus);
    for (let i = 0; i < 10; i++) {
      this.host.maybeRefreshUserStatus();
      await sleep(100);
    }
  }

  async testGetHostCapabilities() {
    assertDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await this.host.getHostCapabilities();
    const expectedCapabilities: HostCapability[] = this.testParams ?? [];
    assertTrue(
        expectedCapabilities.every(
            (expected: HostCapability) => capabilities.has(expected)),
        `Expect each of ${
            this.capabilitiesToString(expectedCapabilities)} is in ${
            this.capabilitiesToString(Array.from(capabilities))}`);
  }

  async testGetModelQualityClientIdFeatureEnabled() {
    assertDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await this.host.getHostCapabilities();
    assertTrue(capabilities.has(HostCapability.GET_MODEL_QUALITY_CLIENT_ID));

    assertDefined(this.host.getModelQualityClientId);
    const clientId = await this.host.getModelQualityClientId();
    assertDefined(clientId);
  }


  async testAdditionalContext() {
    const additionalContextPromise = new Promise<void>(resolve => {
      this.host.getAdditionalContext!().subscribe(async context => {
        assertEquals(context.name, 'part with everything');
        assertDefined(context.tabId);
        assertTrue(context.tabId!.length > 0);
        assertDefined(context.frameUrl);
        assertTrue(context.frameUrl!.length > 0);
        assertEquals(context.parts.length, 7);

        const part1 = context.parts[0]!;
        assertDefined(part1.data);
        assertEquals(part1.data!.type, 'text/plain');
        const data1 = new Uint8Array(await part1.data!.arrayBuffer());
        assertEquals(data1.length, 4);
        assertEquals(data1[0], 't'.charCodeAt(0));

        const part2 = context.parts[1]!;
        assertUndefined(part2.data);
        assertDefined(part2.screenshot);
        assertEquals(part2.screenshot!.widthPixels, 10);
        assertEquals(part2.screenshot!.heightPixels, 20);
        assertEquals(part2.screenshot!.mimeType, 'image/png');
        const data2 = new Uint8Array(part2.screenshot!.data);
        assertEquals(data2.length, 4);
        assertEquals(data2[0], 1);

        const part3 = context.parts[2]!;
        assertDefined(part3.webPageData);
        assertEquals(
            part3.webPageData!.mainDocument.innerText, 'some inner text');

        const part4 = context.parts[3]!;
        assertDefined(part4.annotatedPageData);

        const part5 = context.parts[4]!;
        assertDefined(part5.pdf);
        assertDefined(part5.pdf!.pdfData);
        const pdfText = await new Response(part5.pdf!.pdfData!).text();
        assertEquals(pdfText, 'pdf');

        const part6 = context.parts[5]!;
        assertDefined(part6.tabContext);
        assertDefined(part6.tabContext!.tabData);
        assertEquals(part6.tabContext!.tabData!.tabId, '1');
        assertEquals(part6.tabContext!.tabData!.windowId, '2');
        assertEquals(part6.tabContext!.tabData!.url, 'https://google.com/');

        const part7 = context.parts[6]!;
        assertDefined(part7.region);
        assertDefined(part7.region!.rect);
        assertEquals(part7.region!.rect!.x, 10);
        assertEquals(part7.region!.rect!.y, 20);
        assertEquals(part7.region!.rect!.width, 30);
        assertEquals(part7.region!.rect!.height, 40);
        resolve();
      });
    });

    await this.advanceToNextStep();
    await additionalContextPromise;
  }

  async testSwitchConversationToExistingInstance() {
    assertDefined(this.host.registerConversation);
    assertDefined(this.host.switchConversation);
    if (this.testParams === 'first') {
      await this.host.registerConversation(
          {conversationTitle: 'Hello', conversationId: 'id_hello'});
      await this.advanceToNextStep();
    } else if (this.testParams === 'second') {
      assertEquals(
          undefined,
          this.client.panelOpenData.getCurrentValue()?.conversationId);

      // Return and then switch conversation to ensure that ExecuteJsTest
      // completes before the instance is deleted. The instance is deleted
      // during the `switchConversation` call.
      sleep(100).then(() => {
        assertDefined(this.host.switchConversation);
        this.host.switchConversation(
            {conversationTitle: 'Hello', conversationId: 'id_hello'});
      });
    }
  }

  async testNotifyActOnWebCapabilityChanged() {
    assertDefined(this.host.getActOnWebCapability);
    const actOnWebCapabilitySequence =
        observeSequence(this.host.getActOnWebCapability());
    await actOnWebCapabilitySequence.waitForValue(true);
    await this.advanceToNextStep();
    await actOnWebCapabilitySequence.waitForValue(false);
  }


  async testPanelWillOpenBeforeClientReady() {
    const openData = await observeSequence(this.client.panelOpenData).next();
    assertEquals('test_conversation_id', openData.conversationId);
    assertEquals(
        'Test Conversation Title',
        openData.conversationInfo?.conversationTitle);
    assertEquals(
        'test_client_data_from_cc', openData.conversationInfo?.clientData);
  }

  async testPanelWillOpenHasRecentlyActiveConversations() {
    assertDefined(this.host.registerConversation);

    if (this.testParams === 'instance1') {
      await this.host.registerConversation(
          {conversationTitle: 'Title 1', conversationId: 'convo1'});
    } else if (this.testParams === 'instance2') {
      await this.host.registerConversation(
          {conversationTitle: 'Title 2', conversationId: 'convo2'});
    } else if (this.testParams === 'instance3') {
      await this.host.registerConversation(
          {conversationTitle: 'Title 3', conversationId: 'convo3'});
    } else if (this.testParams === 'instance4') {
      await this.host.registerConversation(
          {conversationTitle: 'Title 4', conversationId: 'convo4'});
    } else if (this.testParams === 'verify') {
      const openData = await observeSequence(this.client.panelOpenData).next();
      assertDefined(openData.recentlyActiveConversations);
      // Expecting convo4, convo2, convo3 (based on activation order in C++
      // test)
      assertEquals(3, openData.recentlyActiveConversations.length);
      assertEquals(
          'convo4', openData.recentlyActiveConversations[0]?.conversationId);
      assertEquals(
          'Title 4',
          openData.recentlyActiveConversations[0]?.conversationTitle);
      assertEquals(
          'convo2', openData.recentlyActiveConversations[1]?.conversationId);
      assertEquals(
          'Title 2',
          openData.recentlyActiveConversations[1]?.conversationTitle);
      assertEquals(
          'convo3', openData.recentlyActiveConversations[2]?.conversationId);
      assertEquals(
          'Title 3',
          openData.recentlyActiveConversations[2]?.conversationTitle);
    }
  }

  async testPanelWillOpenHasPromptSuggestion() {
    const invokeOptions = await observeSequence(this.client.invokeData).next();
    assertEquals('Prompt Suggestion', invokeOptions.prompts?.[0]);
  }


  private async closePanelAndWaitUntilInactive() {
    assertDefined(this.host.closePanel);
    await this.host.closePanel();
    await observeSequence(this.host.panelActive()).waitForValue(false);
  }

  private capabilitiesToString(capabilities: HostCapability[]): string {
    return `[${capabilities.map(this.capabilityToString).join(',')}]`;
  }

  private capabilityToString(capability: HostCapability): string {
    const capabilityName = HostCapability[capability];
    if (capabilityName) {
      return capabilityName;
    }
    throw new Error(`Unknown capability: ${capability}`);
  }
}

class DaisyChainApiTests extends ApiTestFixtureBase {
  async clickLinkInGlicUi() {
    const link = document.createElement('a');
    link.setAttribute('href', location.href);
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();
  }

  // Helper to handle the daisy chain actions.
  async handleDaisyChainStep(action: string) {
    await this.client.waitForInitialize();
    await this.client.waitForFirstOpen();

    if (action === 'createTab') {
      await this.clickLinkInGlicUi();
    } else if (action === 'inputSubmitted') {
      assertDefined(this.host.getMetrics);
      const metrics = this.host.getMetrics();
      assertDefined(metrics);
      assertDefined(metrics.onUserInputSubmitted);
      metrics.onUserInputSubmitted(WebClientMode.TEXT);
    } else {
      assertTrue(false, `Unexpected daisy chain action: ${action}`);
    }
  }

  async testDaisyChainRecursiveAndInput() {
    await this.handleDaisyChainStep(this.testParams);
  }

  async testNewTabMetrics() {
    await this.handleDaisyChainStep(this.testParams);
  }
}


// All test fixtures. We look up tests by name, and the fixture name is ignored.
// Therefore all tests must have unique names.
const TEST_FIXTURES = [
  ApiTests,
  DaisyChainApiTests,
];

testMain(TEST_FIXTURES);
