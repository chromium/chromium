// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {HostCapability, PanelStateKind} from '/glic/glic_api/glic_api.js';
import type {TabData} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, checkDefined, mapObservable, observeSequence, testMain} from './browser_test_base.js';
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
}

// All test fixtures. We look up tests by name, and the fixture name is ignored.
// Therefore all tests must have unique names.
const TEST_FIXTURES = [
  ApiTests,
];

testMain(TEST_FIXTURES);
