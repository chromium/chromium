// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {HostCapability, MetricUserInputReactionType, PanelStateKind, ResponseStopCause, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {TabData} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertNotEquals, assertTrue, checkDefined, mapObservable, observeSequence, testMain} from './browser_test_base.js';
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
