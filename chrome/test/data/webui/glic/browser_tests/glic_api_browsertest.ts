// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {HostCapability, PanelStateKind} from '/glic/glic_api/glic_api.js';
import type {TabData} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertTrue, checkDefined, mapObservable, observeSequence, testMain} from './browser_test_base.js';
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
}

// All test fixtures. We look up tests by name, and the fixture name is ignored.
// Therefore all tests must have unique names.
const TEST_FIXTURES = [
  ApiTests,
];

testMain(TEST_FIXTURES);
