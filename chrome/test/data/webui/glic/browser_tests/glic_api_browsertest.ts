// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/host/glic_api_browsertest.cc

import {CancelActionsResult, ClientCapabilities, ExperimentalTriggeringUpdateType, FileUploadPolicyState, FormFactor, HostCapability, InvocationSource, MetricUserInputReactionType, PanelStateKind, Platform, PromptType, ResponseStopCause, SbThreatType, ScreenshotEncryptionScheme, ScrollToErrorReason, SkillSource, SkillsWebClientEvent, WebClientMode, WebUseCounter} from '/glic/glic_api/glic_api.js';
import type {AdditionalContext, CounterAbuseVerdict, ExperimentalTriggeringUpdate, FocusedTabData, GetPinCandidatesOptions, GlicBrowserHost, GlicWebClient, InvokeOptions, Observable, Observable2, OpenPanelInfo, PageMetadata, PanelOpeningData, PanelState, ScrollToError, TabContextResult, TabData, UserConfirmationDialogRequest, UserProfileInfo, ZeroStateSuggestionsV2} from '/glic/glic_api/glic_api.js';
import type {GlicBrowserHostImpl} from '/glic/glic_api_impl/client/glic_api_client.js';
import {Subject} from '/glic/observable.js';

import {ApiTestError, ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertNotEquals, assertRejects, assertTrue, assertUndefined, checkDefined, mapObservable, observeSequence, readStream, runUntil, sleep, testMain, waitFor, WebClient} from './browser_test_base.js';
import type {SequencedSubscriber} from './browser_test_base.js';

class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testDoNothing() {}

  async testRecordUseCounter() {
    assertDefined(this.host.getMetrics);
    const metrics = this.host.getMetrics();
    assertDefined(metrics);
    assertDefined(metrics.onRecordUseCounter);
    metrics.onRecordUseCounter(WebUseCounter.SUBMIT_PROMPT_WITH_AUTO_MODE);
  }

  async testDefaultInvocationSource() {
    const panelOpenData =
        checkDefined(this.client.panelOpenData.getCurrentValue());
    assertEquals(
        panelOpenData.invocationSource, InvocationSource.TOP_CHROME_BUTTON);
  }

  async testIsBrowserOpen() {
    assertDefined(this.host.isBrowserOpen);
    const isBrowserOpen = observeSequence(this.host.isBrowserOpen());
    assertTrue(await isBrowserOpen.next());
    // Close the browser.
    await this.advanceToNextStep();
    assertTrue(!await isBrowserOpen.next());
  }

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

  async testNavigateToBadPage() {
    const params = this.testParams as {step: string};
    const url = new URL(window.location.href);

    if (params.step === 'trigger_navigation') {
      // A regular web page with no client.
      url.pathname = '/test_data/page.html';
      (async () => {
        await sleep(100);
        location.href = url.toString();
      })();
      return;
    }

    if (params.step === 'verify_fallback') {
      assertEquals(url.pathname, '/glic/browser_tests/test.html');
    }
  }

  async testNavigateToAboutBlank() {
    // Navigation to about:blank will destroy this test client, so the code
    // below will first allow this test function to return, and then navigate.
    (async () => {
      await sleep(100);
      location.href = 'about:blank';
    })();
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

  async testMaybeRefreshUserStatus() {
    assertDefined(this.host.maybeRefreshUserStatus);
    await this.host.maybeRefreshUserStatus();
  }

  async testMaybeRefreshUserStatusThrottled() {
    assertDefined(this.host.maybeRefreshUserStatus);
    for (let i = 0; i < 10; i++) {
      this.host.maybeRefreshUserStatus();
      await sleep(100);
    }
  }
  async testGetModelQualityClientIdFeatureDisabled() {
    assertDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await this.host.getHostCapabilities();
    assertFalse(capabilities.has(HostCapability.GET_MODEL_QUALITY_CLIENT_ID));

    assertUndefined(this.host.getModelQualityClientId);
  }

  async testGetModelQualityClientIdFeatureEnabled() {
    assertDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await this.host.getHostCapabilities();
    assertTrue(capabilities.has(HostCapability.GET_MODEL_QUALITY_CLIENT_ID));

    assertDefined(this.host.getModelQualityClientId);
    const clientId: string = await this.host.getModelQualityClientId();
    assertDefined(clientId);
  }

  async testGetFocusedTabStateV2BrowserClosed() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    // Ignore the initial focus.
    await sequence.next();
    await this.advanceToNextStep();
    const focus = await sequence.next();
    assertFalse(!!focus.hasFocus);
    assertDefined(focus.hasNoFocus);
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

  // Asserts that there is an active tab, and returns its tab ID.
  getActiveTabId(): string {
    const focus =
        checkDefined(this.host.getFocusedTabStateV2?.().getCurrentValue());
    return checkDefined(
        focus.hasFocus?.tabData.tabId ??
        focus.hasNoFocus?.tabFocusCandidateData?.tabId);
  }

  getFocusedTabId(): string {
    assertDefined(this.host.getFocusedTabStateV2);
    const focus = this.host.getFocusedTabStateV2().getCurrentValue();
    return checkDefined(focus?.hasFocus?.tabData.tabId);
  }

  observeActiveTab(): SequencedSubscriber<TabData|undefined> {
    return observeSequence(
        mapObservable(this.host.getFocusedTabStateV2!(), (focus) => {
          return focus?.hasFocus?.tabData ??
              focus?.hasNoFocus?.tabFocusCandidateData;
        }));
  }

  async testPinTabs() {
    // Pin the focused tab and verify it's sent.
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);
    await this.pinActiveTab();

    // Unpin and verify the pinned tab list is updated.
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    const tabId = checkDefined((await pinnedTabsUpdates.next())[0]?.tabId);
    assertTrue(await this.host.unpinTabs([tabId]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testOpenPinnedTabPicker() {
    assertDefined(this.host.openPinnedTabPicker);
    // Verifies that calling openPinnedTabPicker resolves cleanly without error
    // on non-mobile test platforms (where it is currently a no-op).
    // TODO(crbug.com/548681335): Augment with end-to-end assertions once
    // Android picker mocking/delegation is testable in browser tests.
    await this.host.openPinnedTabPicker();
    await this.host.openPinnedTabPicker({});
  }

  async testPinTabsFailsWhenDoesNotExist() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);

    const tabId = this.getFocusedTabId();
    const nonExistTabId = 'not-exist';
    // Pinning a non existing tab id should fail.
    assertFalse(await this.host.pinTabs([tabId, nonExistTabId]));

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 && tabs.some(t => t.tabId === tabId));

    // Un-pinning a non existing tab id should fail.
    assertFalse(await this.host.unpinTabs([tabId, nonExistTabId]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testPinTabsStatePersistWhenClientRestarts() {
    const isFirstRun: boolean = (this.testParams as any).isFirstRun;

    if (isFirstRun) {
      assertDefined(this.host.pinTabs);
      assertDefined(this.host.getPinnedTabs);

      const tabId = (this.testParams as any).tabId;

      assertTrue(await this.host.pinTabs([tabId]));
      const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
      await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);
    } else {
      assertEquals(this.host.getPinnedTabs?.().getCurrentValue()?.length, 2);
    }
  }

  async testPinTabsStatePersistWhenClosePanelAndReopen() {
    assertDefined(this.host.closePanel);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);

    const tabId = (this.testParams as any).tabId;

    assertTrue(await this.host.pinTabs([tabId]));
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    await this.host.closePanel();

    // Open glic window again.
    await this.advanceToNextStep();

    assertEquals(this.host.getPinnedTabs().getCurrentValue()?.length, 2);
  }

  async testUnpinTabsFailsWhenNotPinned() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);

    const tabId = this.testParams.tabId;

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    // Unpin tabId.
    assertTrue(await this.host.unpinTabs([tabId]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);

    // Unpinning a tab that is not pinned should fail.
    assertFalse(await this.host.unpinTabs([tabId]));
  }

  async testUnpinAllTabs() {
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinAllTabs);

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    // Unpin all tabs.
    this.host.unpinAllTabs();
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testPinTabsHaveNoEffectOnFocusedTab() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.unpinAllTabs);

    const tabId1 = (this.testParams as any).tabId1;
    const tabId2 = (this.testParams as any).tabId2;

    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.setTabContextPermissionState);

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs!());
    // Initially, only the active tab (tabId2) is auto-pinned.
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);

    // Focused tab should be tabId2, which is active and pinned.
    const focusedTabUpdates =
        observeSequence(this.host.getFocusedTabStateV2!());
    await focusedTabUpdates.waitFor(
        (focus) => focus?.hasFocus?.tabData.tabId === tabId2);

    await this.host.setTabContextPermissionState(true);

    // Pin first tab (tabId1) which is in the background.
    assertTrue(await this.host.pinTabs([tabId1]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    // Focused tab should still be tabId2.
    assertEquals(
        this.host.getFocusedTabStateV2!
        ().getCurrentValue()
            ?.hasFocus?.tabData.tabId,
        tabId2);

    // Unpin all.
    await this.host.unpinAllTabs();
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testUnpinTabsThatNavigateInBackground() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.closePanel);

    const tabId = (this.testParams as any).tabId;
    // Pin first_tab (background tab).
    assertTrue(await this.host.pinTabs([tabId]));

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs!());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    // Wait for the background tab to navigate. It should stay pinned.
    await this.advanceToNextStep();

    assertEquals(this.host.getPinnedTabs!().getCurrentValue()?.length, 2);

    // Close the panel.
    await this.host.closePanel();

    // The background tab will navigate again. It should be unpinned.
    await this.advanceToNextStep();

    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);
  }

  async testGetContextFromTabIgnorePermissionWhenPinned() {
    assertDefined(this.host.getContextFromTab);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);

    // Fail getContextFromTab due to no tab context permission not granted.
    await this.host.setTabContextPermissionState(false);
    const tabId: string = this.getFocusedTabId();
    await this.host.unpinTabs([tabId]);  // Unpin required for multi-instance.
    await assertRejects(this.host.getContextFromTab(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied:' +
          ' context permission not enabled',
    });

    // Pinning the tab should allow ignoring the tab context permission.
    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 && tabs.some((t) => t.tabId === tabId));

    const result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(result.tabData.tabId, tabId);
  }

  async testGetContextFromTabFailDifferentlyBasedOnPermission() {
    assertDefined(this.host.getContextFromTab);

    const tabId: string = this.testParams.tabId;
    // Make sure tabId is not the focused tab.
    assertNotEquals(tabId, this.getFocusedTabId());

    await this.host.setTabContextPermissionState(false);
    await assertRejects(this.host.getContextFromTab(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied:' +
          ' context permission not enabled',
    });

    await this.host.setTabContextPermissionState(true);
    await assertRejects(this.host.getContextFromTab(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied',
    });
  }

  async testGetContextFromTabFailsIfNotPinned() {
    assertDefined(this.host.getContextFromTab);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.unpinTabs);
    assertDefined(this.host.getPinnedTabs);

    const tabId: string = this.testParams.tabId;
    // Make sure tabId is not the focused tab.
    assertNotEquals(tabId, this.getFocusedTabId());

    // Initially, only the active tab is auto-pinned.
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);

    await this.host.pinTabs([tabId]);
    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 2 && tabs.some((t) => t.tabId === tabId));

    const result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(result.tabData.tabId, tabId);

    await this.host.unpinTabs([tabId]);
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);
    await assertRejects(this.host.getContextFromTab(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied:' +
          ' context permission not enabled',
    });
  }

  async testGetContextFromTabFailsIfDoesNotExist() {
    assertDefined(this.host.onModeChange);
    assertDefined(this.host.getContextFromTab);

    this.host.onModeChange(WebClientMode.TEXT);

    await assertRejects(
        this.host.getContextFromTab('not-exist', {}),
        {withErrorMessage: 'tabContext failed: tab not found'},
    );
  }

  async testGetContextFromPinnedTabWithoutPermission() {
    assertDefined(this.host.getContextFromTab);
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getHostCapabilities);
    await this.host.setTabContextPermissionState(false);

    const focusSequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await focusSequence.next();
    const tabId = checkDefined(focus?.hasFocus?.tabData.tabId);

    // Tab is already pinned in multi-instance mode.
    if (!this.host.getHostCapabilities().has(HostCapability.MULTI_INSTANCE)) {
      assertTrue(await this.host.pinTabs([tabId]));
    }

    const result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(
        new URL(result.tabData.url).pathname, '/test_data/page.html',
        `Tab data has unexpected url ${result.tabData.url}`);
  }

  async testGetContextForActorFromTabWithoutPermission() {
    assertDefined(this.host.getContextForActorFromTab);
    assertDefined(this.host.getFocusedTabStateV2);
    await this.host.setTabContextPermissionState(true);
    const focusSequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await focusSequence.next();
    const tabId: string = checkDefined(focus?.hasFocus?.tabData.tabId);
    await this.host.setTabContextPermissionState(false);
    const result: TabContextResult =
        await this.host.getContextForActorFromTab(tabId, {});
    assertDefined(result);
  }

  async testGetContextForActorFromTabWithRestrictedUrl() {
    assertDefined(this.host.getContextForActorFromTab);
    assertDefined(this.host.getFocusedTabStateV2);
    await this.host.setTabContextPermissionState(true);
    const focusSequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await focusSequence.next();
    const tabId: string =
        checkDefined(focus?.hasNoFocus?.tabFocusCandidateData?.tabId);
    await assertRejects(this.host.getContextForActorFromTab(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied',
    });
  }

  async testGetContextFromFocusedTabWithPdfFile() {
    assertDefined(this.host.getContextFromFocusedTab);
    await this.host.setTabContextPermissionState(true);

    // Pdf pages have two loads: one of the WebContents, and another of the
    // element within an iframe that contains the actual pdf. We need to wait
    // for both to be finished before running the test. The cpp side waits for
    // the WebContents to be loaded, but we must still wait here.
    const result: TabContextResult = await runUntil(async () => {
      const result = await this.host.getContextFromFocusedTab!({pdfData: true});
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
    const pdfData: Uint8Array =
        await readStream(result.pdfDocumentData!.pdfData!);
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

  async testIsOnboardingCompleted() {
    assertDefined(this.host.isOnboardingCompleted);
    const completedSequence =
        observeSequence<boolean>(this.host.isOnboardingCompleted());
    assertFalse(await completedSequence.next());

    // Mark onboarding as completed.
    await this.advanceToNextStep();

    assertTrue(await completedSequence.next());
  }

  async testSetOnboardingCompleted() {
    assertDefined(this.host.setOnboardingCompleted);

    // Check that onboarding is not completed yet.
    await this.advanceToNextStep();

    // Call mojo to set onboarding completed.
    await this.host.setOnboardingCompleted();

    // Check that onboarding is completed.
    await this.advanceToNextStep();
  }

  async testOpenOsMediaPermissionSettings() {
    assertDefined(this.host.openOsPermissionSettingsMenu);
    this.host.openOsPermissionSettingsMenu('media');
  }

  async testOpenOsGeoPermissionSettings() {
    assertDefined(this.host.openOsPermissionSettingsMenu);
    this.host.openOsPermissionSettingsMenu('geolocation');
  }

  async testGetOsMicrophonePermissionStatusAllowed() {
    assertDefined(this.host.getOsMicrophonePermissionStatus);
    assertTrue(await this.host.getOsMicrophonePermissionStatus());
  }

  async testGetOsMicrophonePermissionStatusNotAllowed() {
    assertDefined(this.host.getOsMicrophonePermissionStatus);
    assertFalse(await this.host.getOsMicrophonePermissionStatus());
  }

  async testGetOsHotkeyState() {
    assertDefined(this.host.getOsHotkeyState);
    const osHotkeyState = observeSequence(this.host.getOsHotkeyState());
    let hotkeyState = await osHotkeyState.next();
    assertEquals(this.testParams.expectedHotkey, hotkeyState.hotkey);
    await this.advanceToNextStep();
    hotkeyState = await osHotkeyState.next();
    assertEquals(this.testParams.expectedHotkey, hotkeyState.hotkey);
    await this.advanceToNextStep();
    hotkeyState = await osHotkeyState.next();
    assertEquals(this.testParams.expectedHotkey, hotkeyState.hotkey);
  }

  async testGetFocusedTabStateV2WithNavigation() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus: FocusedTabData = await sequence.next();
    assertDefined(focus.hasFocus);
    assertEquals(
        new URL(focus.hasFocus.tabData.url).pathname, '/test_data/page.html',
        `url=${focus.hasFocus.tabData.url}`);
    assertFalse(!!focus.hasNoFocus);

    // After a second navigation occurs.
    await this.advanceToNextStep();
    const focus2: FocusedTabData = await sequence.next();
    assertDefined(focus2.hasFocus);
    assertEquals(
        new URL(focus2.hasFocus.tabData.url).pathname, '/test_data/page2.html',
        `url=${focus2.hasFocus.tabData.url}`);

    await this.advanceToNextStep();
    let focus3: FocusedTabData = await sequence.next();

    // After a navigation occurs in a new tab, there could first exist a
    // transitory states where the focus is not yet available, is empty, or
    // still previous page.
    while (focus3.hasNoFocus ||
           (!!focus3.hasFocus &&
            (focus3.hasFocus.tabData.url === '' ||
             focus3.hasFocus.tabData.url.endsWith('page2.html')))) {
      focus3 = await sequence.next();
    }

    // Final state, after the tab is fully loaded.
    assertDefined(focus3.hasFocus);
    assertEquals(
        new URL(focus3.hasFocus.tabData.url).pathname,
        '/glic/browser_tests/test.html', `url=${focus3.hasFocus.tabData.url}`);
    assertFalse(!!focus3.hasNoFocus);
  }

  async testGetFocusedTabStateV2WithNavigationWhenInactive() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus: FocusedTabData = await sequence.next();
    assertDefined(focus.hasFocus);
    assertEquals(
        new URL(focus.hasFocus.tabData.url).pathname, '/test_data/page.html',
        `url=${focus.hasFocus.tabData.url}`);
    assertFalse(!!focus.hasNoFocus);

    // After Glic is closed, navigation occurs.
    await this.advanceToNextStep();

    // The client should receive the updated state even while closed (since it's
    // kept alive).
    const focus2: FocusedTabData = await sequence.next();
    assertDefined(focus2.hasFocus);
    assertEquals(
        new URL(focus2.hasFocus.tabData.url).pathname, '/test_data/page2.html',
        `url=${focus2.hasFocus.tabData.url}`);

    // Reopen the panel.
    await this.advanceToNextStep();
  }

  async testSingleFocusedTabUpdatesOnTabEvents() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());

    // #1: Initial state has the first tab open.
    {
      const focus: FocusedTabData = await sequence.next();
      assertDefined(focus.hasFocus);
      assertEquals(
          new URL(focus.hasFocus.tabData.url).pathname, '/test_data/page.html',
          `url=${focus.hasFocus.tabData.url}`);
      assertFalse(!!focus.hasNoFocus);
      assertTrue(sequence.isEmpty());
    }

    // #2: After navigation in the first tab.
    {
      await this.advanceToNextStep();
      const focus: FocusedTabData = await sequence.next();
      assertDefined(focus.hasFocus);
      assertEquals(
          new URL(focus.hasFocus.tabData.url).pathname, '/test_data/page2.html',
          `url=${focus.hasFocus.tabData.url}`);
      assertFalse(!!focus.hasNoFocus);
      assertTrue(sequence.isEmpty());
    }

    // #3: After a second tab is created and focused.
    {
      await this.advanceToNextStep();
      // Tab creation and activation triggers transient deactivation and load
      // states (sending hasNoFocus) before the tab is pinned and fully loaded.
      const focus = await sequence.waitFor(
          f => !!f.hasFocus && f.hasFocus.tabData.url.endsWith('page.html'));
      assertDefined(focus.hasFocus);
      assertEquals(
          new URL(focus.hasFocus.tabData.url).pathname, '/test_data/page.html',
          `url=${focus.hasFocus.tabData.url}`);
      assertFalse(!!focus.hasNoFocus);
      assertTrue(sequence.isEmpty());
    }
  }

  async testGetZoomLevel() {
    assertDefined(this.host.getZoomLevel);
    const zoomLevelSequence = observeSequence(this.host.getZoomLevel());
    const initialZoom = await zoomLevelSequence.next();
    // Verify that the initial zoom is around 1.0.
    assertTrue(
        initialZoom > 0.8 && initialZoom < 1.2,
        `Initial zoom is unexpected: ${initialZoom}`);

    await this.advanceToNextStep();

    const newZoom = await zoomLevelSequence.next();
    // Verify that zoom increased.
    assertTrue(
        newZoom > initialZoom,
        `Zoom did not increase: initial=${initialZoom}, new=${newZoom}`);
    // Verify that the zoom change is reasonable (e.g. around 10% change, but
    // can be smaller due to display scaling).
    const diff = newZoom - initialZoom;
    assertTrue(
        diff > 0.005 && diff < 0.3, `Zoom change is unexpected: diff=${diff}`);
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

  async testGetContextFromFocusedTabWithoutPermission() {
    assertDefined(this.host.onModeChange);
    this.host.onModeChange(WebClientMode.AUDIO);

    assertDefined(this.host.unpinTabs);
    const tabId = this.getFocusedTabId();
    await this.host.unpinTabs([tabId]);

    assertDefined(this.host.getContextFromFocusedTab);
    await assertRejects(this.host.getContextFromFocusedTab({}), {
      withErrorMessage: 'tabContext failed: permission denied:' +
          ' context permission not enabled',
    });
  }

  async testGetContextFromFocusedTabWithNoRequestedData() {
    assertDefined(this.host.getContextFromFocusedTab);
    const result = await this.host.getContextFromFocusedTab({});
    assertDefined(result);
    // tabData is present, but pageContent and screenshot are not.
    assertDefined(result.tabData);
    assertEquals(
        new URL(result.tabData.url).pathname, '/glic/browser_tests/test.html',
        `Tab data has unexpected url ${result.tabData.url}`);
    assertFalse('pageContent' in result);
    assertFalse('screenshot' in result);
  }

  async testGetContextFromFocusedTabWithAllRequestedData() {
    assertDefined(this.host.getContextFromFocusedTab);
    const result = await this.host.getContextFromFocusedTab({
      innerText: true,
      viewportScreenshot: true,
      annotatedPageContent: true,
      maxMetaTags: 32,
      pdfData: true,
    });
    assertDefined(result);
    assertDefined(result.tabData);
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
  async testPinTabsFailsWhenIncognitoWindow() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);

    assertFalse(await this.host.pinTabs([this.testParams.incognitoTabId]));

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    // The active tab is auto-pinned (length = 1), but the incognito tab cannot
    // be pinned.
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);
  }

  async testUnpinTabsWhileClosing() {
    assertDefined(this.host.closePanel);
    const tabId = await this.pinActiveTab();
    const {promise, resolve} = Promise.withResolvers<boolean>();
    this.client.onNotifyPanelWasClosed = () => {
      this.host.unpinTabs!([tabId]).then(resolve);
    };
    await this.host.closePanel();
    assertTrue(await promise);
  }

  async testPinTabsWithTwoTabs() {
    // Pin the focused tab and verify it's sent.
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);
    assertDefined(this.host.getFocusedTabStateV2);

    const tabId = await this.pinActiveTab();

    // Focus the next tab.
    await this.advanceToNextStep();

    // Wait for active tab to change and pin the focused tab.
    await this.observeActiveTab().waitFor((f) => f?.tabId !== tabId);
    const tabId2 = await this.pinActiveTab();

    // Wait until we see two pinned tabs.
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    assertTrue(await this.host.unpinTabs([tabId, tabId2]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);

    // Detach / Close the tab.
    await this.advanceToNextStep();
  }

  async testGetTabByIdWithDiscard() {
    assertDefined(this.host.getTabById);

    // Observe a valid tab id.
    const tabId = this.testParams as string;
    const obs = this.host.getTabById(tabId);
    assertUndefined(obs.getCurrentValue());
    const sequence = observeSequence(obs);
    const tabData = await sequence.next();
    assertEquals(tabId, tabData.tabId);
    assertTrue(
        tabData.url.endsWith('test.html'), `unexpected url: ${tabData.url}`);

    // Discard the tab in C++.
    await this.advanceToNextStep();

    // Navigate the new discarded tab in C++.
    await sequence.waitFor(tabData => tabData.url.endsWith('test.html?q=hi'));

    // Close the tab in C++.
    await this.advanceToNextStep();
    await sequence.waitForComplete();

    // A new subscription should complete without receiving anything.
    const newSeq = observeSequence(this.host.getTabById(tabId));
    await newSeq.waitForComplete();
    assertTrue(newSeq.isEmpty());
  }

  async testGetTabById() {
    assertDefined(this.host.getTabById);

    // Observe an invalid tab id.
    {
      const seq = observeSequence(this.host.getTabById('notA_TabId'));
      await seq.completed;
      assertTrue(seq.isEmpty());
    }

    // Observe a valid tab id that is not found.
    {
      const seq = observeSequence(this.host.getTabById('31415926'));
      await seq.completed;
      assertTrue(seq.isEmpty());
    }

    // Observe a valid tab id.
    {
      const tabId = this.testParams as string;
      const obs = this.host.getTabById(tabId);
      assertUndefined(obs.getCurrentValue());
      const sequence = observeSequence(obs);
      const tabData = await sequence.next();
      assertEquals(tabId, tabData.tabId);
      assertTrue(
          tabData.url.endsWith('test.html'), `unexpected url: ${tabData.url}`);

      // Navigate the tab in C++.
      await this.advanceToNextStep();
      await sequence.waitFor(tabData => tabData.url.endsWith('test.html?q=hi'));

      // Close the tab in C++.
      await this.advanceToNextStep();
      await sequence.waitForComplete();

      // A new subscription should complete without receiving anything.
      const newSeq = observeSequence(this.host.getTabById(tabId));
      await newSeq.waitForComplete();
      assertTrue(newSeq.isEmpty());
    }
  }

  async testUnallowedOriginNavigationBlocked() {}

  async testGetUserProfileInfo() {
    assertDefined(this.host.getUserProfileInfo);
    assertDefined(this.host.getPlatform);
    const profileInfo: UserProfileInfo = await this.host.getUserProfileInfo();
    const platform = await this.host.getPlatform();

    assertEquals('Glic Testing', profileInfo.displayName);
    assertEquals('glic-test@example.com', profileInfo.email);

    assertEquals('Glic', profileInfo.givenName);
    assertEquals(false, profileInfo.isManaged!);
    if (platform !== Platform.CHROME_OS) {
      assertTrue((profileInfo.localProfileName?.length ?? 0) > 0);
      // Can be 'Your Chrome' or 'Your Chromium'.
      assertEquals('Your C', profileInfo.localProfileName?.substring(0, 6));
    }
  }

  async testGetUserProfileInfoCached() {
    assertDefined(this.host.getUserProfileInfo);
    assertDefined(this.host.getPlatform);

    // 1. Fetch the profile (non-cached).
    const profileInfo1: UserProfileInfo = await this.host.getUserProfileInfo();

    // Verify basic data validity.
    assertEquals('Glic Testing', profileInfo1.displayName);
    assertEquals('glic-test@example.com', profileInfo1.email);

    // 2. Fetch the profile again (cached).
    const profileInfo2: UserProfileInfo = await this.host.getUserProfileInfo();

    // 3. Verify that the returned object is the *same instance* as the first
    // one.
    assertTrue(
        profileInfo1 === profileInfo2,
        'Expected cached profile object identity to match');

    // 4. Verify Avatar Blob Caching (Lazy Loading).
    if (profileInfo1.avatarIcon) {
      const avatarPromise1: Promise<Blob|undefined> = profileInfo1.avatarIcon();
      const avatarPromise2: Promise<Blob|undefined> = profileInfo1.avatarIcon();

      // Ensure that the implementation caches the promise itself.
      assertTrue(
          avatarPromise1 === avatarPromise2,
          'Expected avatar promise identity to match');

      const blob1: Blob|undefined = await avatarPromise1;

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

  async testErrorShownOnMojoPipeError() {
    // Calling getModelQualityClientId triggers a mojo pipe error because the
    // runtime feature is disabled.
    (this.host as GlicBrowserHostImpl)
        .clientRemote.requestWithResponse('getModelQualityClientId', undefined);
  }

  async testPanelActiveWithMicrophone() {
    await this.advanceToNextStep();
    await this.advanceToNextStep();
  }

  async testRequestHeader() {
    const rpcUrls: string[] = this.testParams.rpcUrls;
    await Promise.all(rpcUrls.map(url => fetch(url)));
  }

  async testDialogResponseCallOrder() {
    assertDefined(this.host.uninterruptActorTask);
    assertDefined(this.host.createTask);
    assertDefined(this.host.interruptActorTask);
    assertDefined(this.host.selectUserConfirmationDialogRequestHandler);

    // Create a task and subscribe to user confirmation dialog requests.
    const task_id = await this.host.createTask();
    const dialogRequestPromise =
        new Promise<UserConfirmationDialogRequest>((resolve) => {
          assertDefined(this.host.selectUserConfirmationDialogRequestHandler);
          this.host.selectUserConfirmationDialogRequestHandler!
              ().subscribe((request: UserConfirmationDialogRequest) => {
                resolve(request);
              });
        });

    // Wait for the C++ side to request a dialog.
    await this.advanceToNextStep();
    const request: UserConfirmationDialogRequest = await dialogRequestPromise;

    // Respond to the dialog request and then uninterrupt the actor task. The
    // C++ side will check that the dialog response and uninterrupt happen in
    // the called order.
    assertDefined(request);
    request.onDialogClosed({response: {permissionGranted: false}});

    // TODO(b/477060111): This test fails without this. Because onDialogClosed
    // resolves a promise, it doesn't actually postMessage the response until a
    // yield to the event loop. It should probably return a promise which can be
    // awaited. This await yields allowing the queued task that does the
    // postMessage to schedule so that the response message is sent before
    // uninterruptActorTask.
    await new Promise((resolve) => void setTimeout(resolve, 0));

    this.host.uninterruptActorTask(task_id);
  }

  async testPopupOpens() {
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');

    // Attach a click listener to force opening as a popup with specific
    // dimensions. Including features like width/height forces a new window
    // instead of a tab.
    link.addEventListener('click', (e) => {
      e.preventDefault();
      window.open(
          link.getAttribute('href')!, 'popup_window',
          'width=500,height=500,scrollbars=yes,resizable=yes');
    });

    document.body.appendChild(link);
    link.click();
  }

  async testOpenGlicSettingsPage() {
    assertDefined(this.host.openGlicSettingsPage);
    this.host.openGlicSettingsPage();
  }

  async testOpenPasswordManagerSettingsPage() {
    assertDefined(this.host.openPasswordManagerSettingsPage);
    this.host.openPasswordManagerSettingsPage();
  }

  async testOpenContactInfoSettingsPage() {
    assertDefined(this.host.openContactInfoSettingsPage);
    this.host.openContactInfoSettingsPage();
  }

  async testSwitchConversationToOldConversationInPlace() {
    assertDefined(this.host.switchConversation);
    await this.host.switchConversation(
        {conversationId: 'A', conversationTitle: 'Title A'});
  }

  async testSwitchConversationToNewConversationInPlace() {
    assertDefined(this.host.switchConversation);
    await this.host.switchConversation();
  }

  async testClosedCaptioning() {
    assertDefined(this.host.getClosedCaptioningSetting);
    assertDefined(this.host.setClosedCaptioningSetting);
    const closedCaptioningState =
        observeSequence(this.host.getClosedCaptioningSetting());
    assertFalse(await closedCaptioningState.next());
    await this.host.setClosedCaptioningSetting(true);
    assertTrue(await closedCaptioningState.next());
  }

  async testRefreshSignInCookies() {
    assertDefined(this.host.refreshSignInCookies);
    await this.host.refreshSignInCookies();
  }

  async testSignInPauseState() {
    assertDefined(this.host.getUserProfileInfo);
    assertDefined(this.host.getPlatform);
    const profileInfo = await this.host.getUserProfileInfo();
    assertEquals('Glic Testing', profileInfo.displayName);
  }

  async testSwitchConversationToOldConversationNewInstance() {
    assertDefined(this.host.switchConversation);
    await this.host.switchConversation(
        {conversationId: 'initial_id', conversationTitle: 'Initial Title'});
    await this.advanceToNextStep();
    await this.host.switchConversation(
        {conversationId: 'A', conversationTitle: 'Title A'});
  }

  async testSwitchConversationToNewConversationNewInstance() {
    assertDefined(this.host.switchConversation);
    await this.host.switchConversation(
        {conversationId: 'initial_id', conversationTitle: 'Initial Title'});
    await this.advanceToNextStep();
    await this.host.switchConversation();
  }

  async testCanAttachPanelToFallbackEmbedder() {
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);
    assertDefined(this.host.canAttachPanel);
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();
    // The opened tab should be pinned.
    await observeSequence(this.host.getPinnedTabs())
        .waitFor(tabs => tabs.length === 2);

    // Detach panel
    this.host.detachPanel!();

    // Verify state.
    await observeSequence(this.host.getPanelState!())
        .waitFor(
            state =>
                state !== undefined && state.kind === PanelStateKind.DETACHED);

    // The user will close the tab in C++.
    await this.advanceToNextStep();

    // The panel should still be detached.
    const panelStateAfterClose = this.host.getPanelState!().getCurrentValue();
    assertDefined(panelStateAfterClose);
    assertEquals(panelStateAfterClose.kind, PanelStateKind.DETACHED);

    // Verify it is possible to attach
    await observeSequence(this.host.canAttachPanel!()).waitForValue(true);
  }

  async testUnresponsive() {
    // Don't respond to responsiveness checks.
    this.client.checkResponsive = () => {
      return new Promise<void>(() => {});
    };
  }

  async testGetFileUploadAllowedCapability() {
    assertTrue(!!this.host.getFileUploadAllowedCapability);
    const allowed =
        this.host.getFileUploadAllowedCapability!().getCurrentValue();
    assertTrue(allowed === FileUploadPolicyState.ENABLED);

    const stateUpdate = new Promise<void>((resolve) => {
      this.host.getFileUploadAllowedCapability!().subscribe((val) => {
        if (val === FileUploadPolicyState.DISABLED) {
          resolve();
        }
      });
    });

    await this.advanceToNextStep();
    await stateUpdate;
  }

  async testGetContextFromFocusedTabWithIframe() {
    await this.host.setTabContextPermissionState(true);

    const result = await this.host.getContextFromFocusedTab?.({
      viewportScreenshot: true,
    });

    assertDefined(result);
    assertEquals(
        new URL(result.tabData.url).pathname, '/browser_tests/test_iframe.html',
        `Tab data has unexpected url ${result.tabData.url}`);

    assertDefined(result.screenshotInfo);
    const bytes = await new Response(result.screenshotInfo).bytes();
    assertTrue(bytes.length > 0, 'screenshotInfo should not be empty');

    const decoded = new TextDecoder().decode(bytes);
    assertTrue(
        decoded.includes('test.html'),
        `screenshotInfo should contain the iframe URL 'test.html', got: ${
            decoded}`);
  }

  async testReloadWebUi() {}

  async testDefaultTabContextApiIsUndefinedWhenFeatureDisabled() {
    assertTrue(this.host.getDefaultTabContextPermissionState === undefined);
  }

  async testGetDefaultTabContextPermissionState() {
    assertDefined(this.host.getDefaultTabContextPermissionState);
    const defaultTabContextState =
        observeSequence(this.host.getDefaultTabContextPermissionState());
    assertTrue(await defaultTabContextState.next() as boolean);
    await this.advanceToNextStep();
    assertFalse(await defaultTabContextState.next() as boolean);
  }

  async testPinOnBind() {
    assertDefined(this.host.getDefaultTabContextPermissionState);
    assertDefined(this.host.getFocusedTabStateV2);
    const defaultTabContextState =
        observeSequence(this.host.getDefaultTabContextPermissionState());
    assertTrue(await defaultTabContextState.next() as boolean);
    assertDefined(this.host.getPinnedTabs);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());

    // The active tab should be automatically pinned on bind.
    const pinnedTabs =
        await pinnedTabsUpdates.waitFor(tabs => tabs.length === 1);
    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const activeTabId = checkDefined(focus.hasFocus?.tabData.tabId);
    assertEquals(pinnedTabs[0]!.tabId, activeTabId);
  }

  async testNoPinOnBindWhenSettingOff() {
    assertDefined(this.host.getPinnedTabs);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());

    // The initial value is an empty array.
    const initialTabs = await pinnedTabsUpdates.next();
    assertEquals(0, initialTabs.length);

    // Wait briefly to ensure no unexpected updates arrive.
    await sleep(200);
    assertTrue(
        pinnedTabsUpdates.isEmpty(),
        'Pinned tabs should remain empty when auto-pinning is disabled.');
  }

  async testWebActuationSettingIsUndefinedWhenFeatureDisabled() {
    assertTrue(this.host.getActuationOnWebSetting === undefined);
  }

  async testGetWebActuationSetting() {
    assertDefined(this.host.getActuationOnWebSetting);
    const webActuationSetting =
        observeSequence(this.host.getActuationOnWebSetting());
    assertFalse(await webActuationSetting.next() as boolean);
    await this.advanceToNextStep();
    assertTrue(await webActuationSetting.next() as boolean);
  }

  async testInvocationSource() {
    const expectedSource = this.testParams as number;
    await observeSequence(this.client.panelOpenData)
        .waitFor((data) => data && data.invocationSource === expectedSource);
  }

  private async assertCreateTabFails(url: string) {
    assertDefined(this.host.createTab);
    await assertRejects(
        this.host.createTab(url, {openInBackground: false}),
        {withErrorMessage: 'createTab: failed'});
  }

  async testCreateTab() {
    assertDefined(this.host.createTab);
    // Open a tab pointing to test.html.
    const url = location.href;
    const data = await this.host.createTab(url, {openInBackground: false});
    assertEquals(data.url, url);
  }

  async testCreateTabSimple() {
    assertDefined(this.host.createTab);
    const url = location.href + '#simple';
    const data = await this.host.createTab(url, {openInBackground: false});
    assertEquals(data.url, url);
  }

  async testActivateTabWithUrl() {
    assertDefined(this.host.createTab);
    assertDefined(this.host.activateTabWithUrl);
    const prodUrl = location.href + '#activate_prod';
    const createdProd = await this.host.createTab(prodUrl, {});
    assertEquals(createdProd.url, prodUrl);
    assertTrue(await this.browser.navigateTab(createdProd.tabId, prodUrl));

    // Open another tab so prodUrl is no longer the active tab.
    const blankUrl = location.href + '#blank';
    const createdBlank = await this.host.createTab(blankUrl, {});
    assertEquals(createdBlank.url, blankUrl);
    assertTrue(await this.browser.navigateTab(createdBlank.tabId, blankUrl));

    // Activating with autopush URL but matching prod pattern should deduplicate
    // and return prod tab.
    const autopushUrl = location.href + '#activate_autopush';
    const activated = await this.host.activateTabWithUrl(
        autopushUrl, {pattern: '*activate_prod*'});
    assertDefined(activated);
    assertEquals(activated.tabId, createdProd.tabId);
    assertEquals(activated.url, prodUrl);

    // Activating a non-existent URL should fall back to creating a new tab
    // without showing the Glic side panel.
    const fallbackUrl = location.href + '&nonexistent=1#activate_fallback';
    const fallbackCreated = await this.host.activateTabWithUrl(
        fallbackUrl, {pattern: '*activate_nonexistent*'});
    assertDefined(fallbackCreated);
    assertEquals(fallbackCreated.url, fallbackUrl);
  }

  async testCreateTabFailsWithUnsupportedScheme() {
    assertDefined(this.host.createTab);

    this.assertCreateTabFails('chrome://settings');
    this.assertCreateTabFails('ftps://www.google.com');
    this.assertCreateTabFails('chrome-extension://www.google.com');
    this.assertCreateTabFails('mailto:user@google.com');
    this.assertCreateTabFails(
        'data:text/html;charset=utf-8,<html>Hello World</html>');
    this.assertCreateTabFails('file:///tmp/test.html');
  }

  async testNoRemoveBlankInstanceOnCloseIfInputSubmitted() {
    assertDefined(this.host.getMetrics);
    const metrics = this.host.getMetrics();
    assertDefined(metrics.onUserInputSubmitted);
    metrics.onUserInputSubmitted(WebClientMode.TEXT);
  }

  async testCreateTabInBackground() {
    assertDefined(this.host.createTab);

    await this.host.createTab(
        location.href + '#foreground', {openInBackground: false});

    await this.advanceToNextStep();

    await this.host.createTab(
        location.href + '#background', {openInBackground: true});
  }

  async testCreateTabByClickingOnLink() {
    assertDefined(this.host.setAudioDucking);
    // Check that audio ducking still works after clicking a link.
    await this.host.setAudioDucking(true);
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();

    await this.advanceToNextStep();
    await this.host.setAudioDucking(false);
  }

  async testCreateTabByClickingOnLinkDaisyChains() {
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.getPinnedTabs);
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();
    // The opened tab should be pinned.
    await observeSequence(this.host.getPinnedTabs())
        .waitFor(tabs => tabs.length === 2);
  }

  async testFailureForCapturedApiTestError() {
    try {
      throw new ApiTestError('Non-throwing test error');
    } catch (e) {
    }
  }

  async testLoadWhileWindowClosed() {
    await observeSequence(this.host.panelActive()).waitForValue(false);
  }

  async testNoWebUiLoader() {}

  async testGetPageMetadata() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    const metadataObservable = this.host.getPageMetadata(tabId, ['author']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    const metadata: PageMetadata = await metadataSequence.next();

    assertEquals(1, metadata.frameMetadata.length);
    const authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('George', authorTag.content);
  }

  /**
   * Ensures that subscribing to metadata for an invalid `tabId` does not
   * result in any emissions.
   */
  async testGetPageMetadataInvalidTabId() {
    assertDefined(this.host.getPageMetadata);

    const metadataObservable =
        this.host.getPageMetadata('invalid-tab-id', ['author']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    // The observable should not emit any values, and should complete.
    await waitFor(metadataSequence.completed);
    assertTrue(metadataSequence.isEmpty());
  }

  /**
   * Confirms that calling `getPageMetadata` with an empty array of meta tag
   * names throws an error, as expected.
   */
  async testGetPageMetadataEmptyNames() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    try {
      this.host.getPageMetadata(tabId, []);
      assertTrue(false, 'Should have thrown an error');
    } catch (e) {
      assertEquals('names must not be empty', (e as Error).message);
    }
  }

  /**
   * Verifies that subsequent calls to `getPageMetadata` for the same `tabId`
   * return the same `ObservableValue` instance, ignoring the new `names`
   * parameter.
   */
  async testGetPageMetadataMultipleSubscriptions() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    const metadataObservable1 = this.host.getPageMetadata(tabId, ['author']);
    assertDefined(metadataObservable1);

    const metadataObservable2 =
        this.host.getPageMetadata(tabId, ['description']);
    assertDefined(metadataObservable2);

    assertTrue(metadataObservable1 === metadataObservable2);
  }

  /**
   * Tests that the `ObservableValue` returned by `getPageMetadata` emits new
   * values when the page's metadata changes.
   */
  async testGetPageMetadataUpdates() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    const metadataObservable = this.host.getPageMetadata(tabId, ['author']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    const metadata: PageMetadata = await metadataSequence.next();
    assertEquals(1, metadata.frameMetadata.length);
    const authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('George', authorTag.content);

    // Change the content of the 'author' meta tag from "George" to "Ruth".
    assertTrue(await this.browser.execJsInTab(tabId, `
      document.querySelector("meta[name='author']").content = 'Ruth';
    `));

    const metadata2: PageMetadata = await metadataSequence.next();
    assertEquals(1, metadata2.frameMetadata.length);
    const authorTag2 =
        metadata2.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag2);
    assertEquals('Ruth', authorTag2.content);
  }

  /**
   * Verifies that getPageMetadata emits new values when the tab navigates to a
   * new page.
   */
  async testGetPageMetadataOnNavigation() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    const metadataObservable =
        this.host.getPageMetadata(tabId, ['author', 'description']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    // The initial page has one meta tag.
    let metadata: PageMetadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    assertEquals(1, metadata.frameMetadata[0]!.metaTags.length);
    const authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('George', authorTag.content);

    // Navigate to a page with no meta tags.
    assertTrue(
        await this.browser.navigateTab(tabId, this.getUrl('/title1.html')));

    metadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    assertEquals(0, metadata.frameMetadata[0]!.metaTags.length);
  }

  /**
   * Verifies that metadata updates are still received after a tab's
   * WebContents has been discarded and recreated.
   */
  async testGetPageMetadataWebContentsChanged() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.createTab);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    const metadataObservable = this.host.getPageMetadata(tabId, ['author']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    let metadata: PageMetadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    let authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('George', authorTag.content);

    // Keep the browser alive by opening another tab.
    await this.host.createTab(location.href, {openInBackground: true});

    // C++ side will discard and reload the tab, then change the meta tag.
    await this.advanceToNextStep();

    // After a WebContents change, we might get intermediate updates (e.g.,
    // empty metadata) before the final, updated value. We loop until we see
    // the expected content.
    while (true) {
      metadata = await metadataSequence.next();
      authorTag = metadata.frameMetadata?.[0]?.metaTags?.find(
          tag => tag.name === 'author');
      if (authorTag?.content === 'Ruth') {
        break;
      }
      console.info(
          `Ignoring intermediate metadata: ${JSON.stringify(metadata)}`);
    }

    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('Ruth', authorTag.content);
  }

  async testGetPageMetadataTabDestroyed() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.getPinnedTabs);

    await observeSequence(this.host.getPinnedTabs())
        .waitFor(t => t.length === 2);
    const focus = await observeSequence(this.host.getFocusedTabStateV2())
                      .waitFor(f => !!f?.hasFocus?.tabData.tabId);
    const focusedTabId = checkDefined(focus.hasFocus?.tabData.tabId);

    let tabs = checkDefined(this.host.getPinnedTabs().getCurrentValue());
    tabs = tabs.filter(t => t.tabId !== focusedTabId);
    const otherTabId = checkDefined(tabs[0]?.tabId);

    const metadataObservable =
        this.host.getPageMetadata(otherTabId, ['author']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    const metadata: PageMetadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata[0]!.metaTags.length);

    await this.browser.closeTab(otherTabId);

    // The observable should not emit any more values, and should complete.
    await waitFor(metadataSequence.completed);
    assertTrue(metadataSequence.isEmpty());
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

  async testCancelActions() {
    assertDefined(this.host.cancelActions);
    // Task with id 12345 does not exist.
    assertEquals(
        await this.host.cancelActions(12345),
        CancelActionsResult.TASK_NOT_FOUND);
  }

  async testRegisterConversationWithEmptyId() {
    assertDefined(this.host.registerConversation);
    // Register an initial conversation with a valid ID.
    await this.host.registerConversation(
        {conversationId: '', conversationTitle: 'Empty Conversation'});
  }

  async testCallingApiWhileHiddenRecordsMetrics() {
    assertDefined(this.host.createTab);
    await this.advanceToNextStep();
    await observeSequence(this.host.panelActive())
        .waitFor(isActive => !isActive);
    try {
      await this.host.createTab(location.href, {openInBackground: true});
    } catch {
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

  async testPageMetadataCrossProfile() {
    const otherTabId = this.testParams as string;
    assertDefined(this.host.getPageMetadata);
    const observable = this.host.getPageMetadata(otherTabId, ['title']);
    const sequence = observeSequence(observable);
    await sequence.waitForComplete();
    assertEquals(
        true, sequence.isEmpty(),
        'Expected no page metadata for cross-profile tab');
  }

  async testTabDataCrossProfile() {
    const otherTabId = this.testParams as string;
    assertDefined(this.host.getTabById);
    const observable = this.host.getTabById(otherTabId);
    const sequence = observeSequence(observable);
    await sequence.waitForComplete();
    assertEquals(
        true, sequence.isEmpty(), 'Expected no tab data for cross-profile tab');
  }

  async testTabFaviconCrossProfile() {
    const otherTabId = this.testParams as string;
    assertDefined(this.host.getTabFaviconById);
    const observable = this.host.getTabFaviconById(otherTabId);
    const sequence = observeSequence(observable);
    await sequence.waitForComplete();
    assertEquals(
        true, sequence.isEmpty(), 'Expected no favicon for cross-profile tab');
  }

  async testGetContextCrossProfile() {
    const otherTabId = this.testParams as string;
    assertDefined(this.host.getContextForActorFromTab);
    await assertRejects(this.host.getContextForActorFromTab(otherTabId, {}), {
      withErrorMessage: 'tabContext failed: profile mismatch',
    });
  }

  async testShowClientErrorDialog() {
    assertDefined(this.host.setErrorDialogState);
    this.host.setErrorDialogState!(1 /* kDisabledByOrganization */);
  }

  async testReportClientTransientError() {
    assertDefined(this.host.reportClientTransientError);
    this.host.reportClientTransientError!(16 /* kUnauthenticated */);
  }

  private async closePanelAndWaitUntilInactive() {
    assertDefined(this.host.closePanel);
    await this.host.closePanel();
    await observeSequence(this.host.panelActive()).waitForValue(false);
  }

  async testGetZeroStateSuggestionsForFocusedTabApi() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsForFocusedTabFailsWhenHidden() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);
  }

  async testNoZssWarmingStateMachine() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testNoZssWarmingStateMachineImplicitPreservesDisabled() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testNoZssWarmingStateMachineImplicitPreservesEnabled() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsApi() {
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
    assertEquals(false, suggestions.isPending);
  }

  async testGetZeroStateSuggestionsUnsubscribeAndResubscribe() {
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence1 = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions1 = await sequence1.next();
    assertDefined(suggestions1);
    assertEquals(3, suggestions1.suggestions.length);

    // Unsubscribe.
    sequence1.unsubscribe();

    // Re-subscribing should work and fetch suggestions without hitting a closed
    // pipe.
    const sequence2 = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions2 = await sequence2.next();
    assertDefined(suggestions2);
    assertEquals(3, suggestions2.suggestions.length);
    assertEquals(false, suggestions2.isPending);
  }

  async testGetZeroStateSuggestionsMultipleNavigations() {
    // Initial state.
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
    assertEquals(
        'Sug 1 for /test_data/page.html',
        suggestions.suggestions[0]?.suggestion);
    assertEquals(false, suggestions.isPending);

    // After a second navigation occurs.
    assertTrue(
        await this.browser.navigateActiveTab(this.getTestUrl('page.html?new')));

    // Should first get a pending state.
    const suggestions2 = await sequence.next();
    assertDefined(suggestions2);
    // We don't care about the suggestions here.
    assertEquals(true, suggestions2.isPending);

    // Should later get the actual suggestions.
    const suggestions3 = await sequence.next();
    assertDefined(suggestions3);
    assertEquals(3, suggestions3.suggestions.length);
    assertEquals(
        'Sug 1 for /test_data/page.html?new',
        suggestions3.suggestions[0]?.suggestion);
    assertEquals(false, suggestions3.isPending);
  }

  async testGetZeroStateSuggestionsFailsWhenHidden() {
    // Initial state.
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);

    // Close panel.
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();

    // After next navigation in focused tab occurs.
    await this.advanceToNextStep();
  }

  async testProcessCounterAbuseVerdict() {
    assertDefined(this.host.processCounterAbuseVerdict);
    assertDefined(this.host.getFocusedTabStateV2);
    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabData = checkDefined(focus.hasFocus?.tabData);
    const url = tabData.url;

    const verdict: CounterAbuseVerdict = {
      sbVerdictResult: {
        url: url,
        threatType: SbThreatType.SOCIAL_ENGINEERING,
        showInterstitial: true,
      },
    };
    this.host.processCounterAbuseVerdict(tabData.tabId, verdict);

    await this.advanceToNextStep();
  }

  async testProcessCounterAbuseVerdictWhenSafeBrowsingDisabled() {
    await this.testProcessCounterAbuseVerdict();
  }

  async testProcessCounterAbuseVerdictWhenUrlAllowlistedByPolicy() {
    await this.testProcessCounterAbuseVerdict();
  }

  async testProcessCounterAbuseVerdictIsUndefinedWhenFeatureDisabled() {
    assertTrue(this.host.processCounterAbuseVerdict === undefined);
  }

  async testScrollToFindsText() {
    assertDefined(this.host.scrollTo);
    assertDefined(this.host.setTabContextPermissionState);
    assertDefined(this.host.setContextAccessIndicator);
    await this.host.setTabContextPermissionState(true);
    this.host.setContextAccessIndicator(true);
    await this.host.scrollTo({
      selector: {exactText: {text: 'Because of the table layout'}},
      highlight: true,
      documentId: this.testParams.documentId,
    });
  }

  async testScrollToFindsTextNoTabContextPermission() {
    assertDefined(this.host.scrollTo);
    try {
      await this.host.scrollTo({
        selector: {exactText: {text: 'Because of the table layout'}},
        highlight: true,
        documentId: this.testParams.documentId,
      });
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.TAB_CONTEXT_PERMISSION_DISABLED,
          (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  async testScrollToFailsWhenInactive() {
    assertDefined(this.host.scrollTo);
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    try {
      await this.host.scrollTo({
        selector: {exactText: {text: 'Because of the table layout'}},
        highlight: true,
        documentId: this.testParams.documentId,
      });
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.NO_FOCUSED_TAB, (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  async testScrollToNoMatchFound() {
    assertDefined(this.host.scrollTo);
    assertDefined(this.host.setTabContextPermissionState);
    assertDefined(this.host.setContextAccessIndicator);
    await this.host.setTabContextPermissionState(true);
    this.host.setContextAccessIndicator(true);
    try {
      await this.host.scrollTo({
        selector: {exactText: {text: 'Abracadabra'}},
        highlight: true,
        documentId: this.testParams.documentId,
      });
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.NO_MATCH_FOUND, (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  async testGetImageBytesFromTab() {
    assertDefined(this.host.getImageBytesFromTab);

    const tabId = checkDefined(this.testParams.tabId);
    const documentId = checkDefined(this.testParams.documentId);
    const domNodeId = checkDefined(this.testParams.domNodeId);

    // Call from tab ID
    const result =
        await this.host.getImageBytesFromTab(tabId, documentId, domNodeId);
    assertDefined(result);
    assertDefined(result.bytes);
    assertTrue(result.bytes.byteLength > 0);
    assertDefined(result.imageInfo);
    assertEquals(result.imageInfo.mimeType, 'image/gif');
    assertEquals(result.imageInfo.caption, 'test_image_bytes');
    assertEquals(result.imageInfo.url, '');
    assertEquals(result.imageInfo.sourceOrigin, 'null');

    // Test failures.
    // 1. Invalid DOM node ID.
    await assertRejects(
        this.host.getImageBytesFromTab(tabId, documentId, 9999),
        {withErrorMessage: 'getImageBytes failed: failed to get image bytes'});

    // 2. Invalid tab ID.
    await assertRejects(
        this.host.getImageBytesFromTab('99999', documentId, domNodeId),
        {withErrorMessage: 'getImageBytes failed: tab not found'});
  }

  // Test getPinCandidates() in some different scenarios where there is a single
  // browser tab.
  async testGetPinCandidatesSingleTab() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinCandidates);
    assertDefined(this.host.getHostCapabilities);

    // Gets pinned candidates and asserts that their comma-separated titles
    // equal `expected`.
    const getCandidatesEquals =
        async (options: GetPinCandidatesOptions, expected: string) => {
      const sequence = observeSequence(this.host.getPinCandidates!(options));
      const candidates = await sequence.next();
      sequence.unsubscribe();
      assertEquals(candidates.map(c => c.tabData.title).join(', '), expected);
    };

    await getCandidatesEquals({maxCandidates: 1}, 'Test Page');
    await getCandidatesEquals({maxCandidates: 1, query: 'zxyzyz'}, 'Test Page');
    await getCandidatesEquals(
        {maxCandidates: 1, query: 'Test Page'}, 'Test Page');
    await getCandidatesEquals({maxCandidates: 0}, '');

    // Test some races.

    // 1. Calling getPinCandidates a second time will reset the first
    // observable. We should receive nothing from it.
    let racedSequence =
        observeSequence(this.host.getPinCandidates!({maxCandidates: 1}));
    await getCandidatesEquals({maxCandidates: 1}, 'Test Page');
    assertTrue(racedSequence.isEmpty());
    racedSequence.unsubscribe();

    // 2. Unsubscribing the obsolete observable should do nothing to the new
    // one.
    racedSequence =
        observeSequence(this.host.getPinCandidates!({maxCandidates: 1}));
    const racedSequence2 =
        observeSequence(this.host.getPinCandidates!({maxCandidates: 1}));
    racedSequence.unsubscribe();
    assertEquals(1, (await racedSequence2.next()).length);

    // Pin the current focus. A pinned tab isn't a valid candidate.
    const focus =
        await observeSequence(this.host.getFocusedTabStateV2!()).next();
    // In multi-instance, only pinned tabs can be considered focused, but the
    // candidate does reveal the active tab.
    if (this.host.getHostCapabilities().has(HostCapability.MULTI_INSTANCE)) {
      await this.host.pinTabs(
          [checkDefined(focus.hasNoFocus?.tabFocusCandidateData?.tabId)]);
    } else {
      await this.host.pinTabs([checkDefined(focus.hasFocus?.tabData.tabId)]);
    }
    await getCandidatesEquals({maxCandidates: 1}, '');
  }

  async testGetPinCandidatesWithPanelClosed() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinCandidates);

    const sequence =
        observeSequence(this.host.getPinCandidates!({maxCandidates: 10}));
    sequence.waitFor(tabs => tabs.length === 1);
    this.host.closePanel!();

    // Open a tab. The client should not receive any updates.
    await this.advanceToNextStep();
    await sleep(500);
    while (!sequence.isEmpty()) {
      assertEquals((await sequence.next()).length, 1);
    }

    // Show the panel again. The client should receive an update.
    await this.advanceToNextStep();
    sequence.waitFor(tabs => tabs.length === 2);
  }

  async testGetFormFactor() {
    assertDefined(this.host.getFormFactor);
    const formFactor = this.host.getFormFactor();
    assertDefined(formFactor);
    if (navigator.userAgent.includes('Android')) {
      if (navigator.userAgent.includes('Mobile')) {
        assertEquals(formFactor, FormFactor.PHONE);
      } else {
        assertTrue(
            formFactor === FormFactor.TABLET ||
            formFactor === FormFactor.DESKTOP);
      }
    } else {
      assertEquals(formFactor, FormFactor.DESKTOP);
    }
  }

  async testGetFocusedTabStateV2() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertDefined(focus.hasFocus);
    assertEquals(
        new URL(focus.hasFocus.tabData.url).pathname,
        '/glic/browser_tests/test.html', `url=${focus.hasFocus.tabData.url}`);
    assertEquals('Test Page', focus.hasFocus.tabData.title);
    assertFalse(!!focus.hasNoFocus);
  }

  async testClosePanel() {
    assertDefined(this.host.closePanel);

    // Close the panel, and verify notifyPanelWasClosed is called.
    const closedPromise = Promise.withResolvers<void>();
    this.client.onNotifyPanelWasClosed = closedPromise.resolve;
    await this.host.closePanel();
    await waitFor(closedPromise.promise);
  }

  async testClosePanelAndShutdown() {
    assertDefined(this.host.closePanelAndShutdown);

    // Close the panel, and verify notifyPanelWasClosed is called.
    const closedPromise = Promise.withResolvers<void>();
    this.client.onNotifyPanelWasClosed = closedPromise.resolve;
    this.host.closePanelAndShutdown();
    await waitFor(closedPromise.promise);
  }

  async testShowProfilePicker() {
    assertDefined(this.host.showProfilePicker);
    this.host.showProfilePicker();
    // There is a problem with InProcessBrowserTest::QuitBrowsers(). Opening the
    // profile picker at the same time as exiting a test results in
    // QuitBrowsers() never exiting. This sleep avoids this problem.
    await sleep(500);
  }

  async testPanelActive() {
    assertDefined(this.host.panelActive);
    const activeSequence = observeSequence(this.host.panelActive());
    assertDefined(this.host.closePanel);
    await this.host.closePanel();
    assertTrue(await activeSequence.next());
    await this.advanceToNextStep();
    assertFalse(await activeSequence.next());
  }

  async testGetPanelStateAttached() {
    assertDefined(this.host.getPanelState);
    // getPanelState and notifyPanelWillOpen should signal the ATTACHED state.
    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);
    assertEquals(
        PanelStateKind.ATTACHED,
        this.client.panelOpenStateKind.getCurrentValue());
    await sleep(100);
    // It should remain in the attached state.
    assertEquals(
        PanelStateKind.ATTACHED,
        this.host.getPanelState().getCurrentValue()?.kind);
  }

  async testGetPanelStateAttachedHidden() {
    assertDefined(this.host.getPanelState);
    // getPanelState and notifyPanelWillOpen should signal the ATTACHED state.
    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    // Open and select a second tab.
    await this.advanceToNextStep();
    await panelStates.waitFor(state => state.kind === PanelStateKind.HIDDEN);

    // Select the first tab again.
    await this.advanceToNextStep();
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);
  }

  async testCanAttachPanelSidePanel() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.canAttachPanel);

    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    await observeSequence(this.host.canAttachPanel()).waitForValue(false);
  }

  async testCanAttachPanelDetached() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);
    assertDefined(this.host.canAttachPanel);

    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    this.host.detachPanel();
    await panelStates.waitFor(state => state.kind === PanelStateKind.DETACHED);

    await observeSequence(this.host.canAttachPanel()).waitForValue(true);
  }

  async testDetachPanel() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);
    assertDefined(this.host.attachPanel);
    // getPanelState and notifyPanelWillOpen should signal the ATTACHED state.
    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    this.host.detachPanel();
    await panelStates.waitFor(state => state.kind === PanelStateKind.DETACHED);

    // TODO(harringtond): Not implemented yet.
    // this.host.attachPanel();
    // await panelStates.waitFor(state => state.kind ===
    //    PanelStateKind.ATTACHED);
  }

  async testDetachPanelNoFloatyOrLiveMode() {
    assertDefined(this.host.getPanelState);
    // getPanelState and notifyPanelWillOpen should signal the ATTACHED state.
    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    assertRejects((async () => {
      this.host.detachPanel?.();
    })());
  }

  async testCanAttachPanelDetachedTabClosed() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);
    assertDefined(this.host.canAttachPanel);

    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    this.host.detachPanel();
    await panelStates.waitFor(state => state.kind === PanelStateKind.DETACHED);

    const canAttachSeq = observeSequence(this.host.canAttachPanel());
    await canAttachSeq.waitForValue(true);

    // Wait for C++ to close the tab.
    await this.advanceToNextStep();

    await canAttachSeq.waitForValue(false);
  }

  async testAttachPanel() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);
    assertDefined(this.host.attachPanel);

    const panelStates = observeSequence(this.host.getPanelState());
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);

    this.host.detachPanel();
    await panelStates.waitFor(state => state.kind === PanelStateKind.DETACHED);

    this.host.attachPanel();
    await panelStates.waitFor(state => state.kind === PanelStateKind.ATTACHED);
  }

  async testMultiplePanelsAttachedAndDetached() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);

    if (this.testParams === 'first') {
      const panelStates = observeSequence(this.host.getPanelState());
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.ATTACHED);
      await this.advanceToNextStep();
      // Ensure the panel state stays attached. Note that currently, we do see
      // the panel state go to hidden momentarily, so we only assert that the
      // state eventually transitions again to attached.
      await sleep(100);
      observeSequence(this.host.getPanelState())
          .waitFor(state => state.kind === PanelStateKind.ATTACHED);
    } else if (this.testParams === 'second') {
      this.host.detachPanel();
      const panelStates = observeSequence(this.host.getPanelState());
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.DETACHED);
    }
  }

  async testThereCanOnlyBeOneFloaty() {
    assertDefined(this.host.getPanelState);
    assertDefined(this.host.detachPanel);

    if (this.testParams === 'first') {
      this.host.detachPanel();
      const panelStates = observeSequence(this.host.getPanelState());
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.DETACHED);
      await this.advanceToNextStep();

      observeSequence(this.host.getPanelState())
          .waitFor(state => state.kind === PanelStateKind.HIDDEN);

    } else if (this.testParams === 'second') {
      this.host.detachPanel();
      const panelStates = observeSequence(this.host.getPanelState());
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.DETACHED);
    }
  }

  async testSwitchConversationWithEmptyId() {
    assertDefined(this.host.registerConversation);
    assertDefined(this.host.switchConversation);

    if (this.testParams === 'initiateSwitch') {
      // Register an initial conversation with a valid ID.
      await this.host.registerConversation(
          {conversationId: 'initial_id', conversationTitle: 'Initial Title'});

      // Attempt to switch to a conversation with an empty ID.
      // Wrap in a sleep to allow the current test's ExecuteJsTest() to complete
      // before the instance is potentially deleted during switchConversation.
      sleep(100).then(() => {
        assertDefined(this.host.switchConversation);
        this.host.switchConversation({
          conversationId: '',
          conversationTitle: 'Empty Switched Title',
          clientData: 'test_client_data_from_ts',
        });
      });
    } else if (this.testParams === 'verifyNewInstance') {
      const openData = this.client.panelOpenData.getCurrentValue();
      assertDefined(openData);
      assertEquals(undefined, openData.conversationId);
      assertEquals('', openData.conversationInfo?.conversationId);
      assertEquals(
          'Empty Switched Title', openData.conversationInfo?.conversationTitle);
      assertEquals(
          'test_client_data_from_ts', openData.conversationInfo?.clientData);
    }
  }

  async testTabSwitchDoesNotLogActivationMetric() {
    assertDefined(this.host.registerConversation);
    assertDefined(this.host.switchConversation);
    if (this.testParams === 'first') {
      await this.host.registerConversation(
          {conversationId: 'A', conversationTitle: 'Title A'});
      this.advanceToNextStep();
    } else if (this.testParams === 'second') {
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

  async testDetachDoesNotLogActivationMetric() {
    assertDefined(this.host.registerConversation);
    assertDefined(this.host.detachPanel);
    assertDefined(this.host.getPanelState);

    if (this.testParams === 'registerAndDetach') {
      await this.host.registerConversation(
          {conversationId: 'A', conversationTitle: 'Title A'});
      const panelStates = observeSequence(this.host.getPanelState());
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.ATTACHED);

      this.host.detachPanel();
      await panelStates.waitFor(
          state => state.kind === PanelStateKind.DETACHED);
    }
  }

  async testActuationOnWebSetting() {
    assertDefined(this.host.getActuationOnWebSetting);
    assertDefined(this.host.setActuationOnWebSetting);
    const actuationOnWebState =
        observeSequence(this.host.getActuationOnWebSetting());
    assertFalse(await actuationOnWebState.next());
    await this.host.setActuationOnWebSetting(true);
    assertTrue(await actuationOnWebState.next());
  }

  async testSetContextAccessIndicator() {
    assertDefined(this.host.setContextAccessIndicator);
    await this.host.setContextAccessIndicator(true);
  }

  async testSetAudioDucking() {
    assertDefined(this.host.setAudioDucking);
    await this.host.setAudioDucking(true);
  }

  async testGeminiEnterpriseSettings() {
    assertDefined(this.host.getGeminiEnterpriseSettings);
    const settingsObservable = this.host.getGeminiEnterpriseSettings();

    const settings = settingsObservable.getCurrentValue();
    assertDefined(settings);
    assertEquals(settings.projectId, 'switch-project');
    assertEquals(settings.appId, 'switch-engine');
    assertEquals(settings.location, 'switch-location');
  }

  async testGeminiEnterpriseSettingsDisabled() {
    assertDefined(this.host.getGeminiEnterpriseSettings);
    const settingsObservable = this.host.getGeminiEnterpriseSettings();
    const settings = settingsObservable.getCurrentValue();
    assertUndefined(settings);
  }

  async testGeminiEnterpriseSettingsPolicy() {
    assertDefined(this.host.getGeminiEnterpriseSettings);
    const settingsObservable = this.host.getGeminiEnterpriseSettings();
    const settings = settingsObservable.getCurrentValue();
    assertDefined(settings);
    assertEquals(settings.projectId, 'policy-project');
    assertEquals(settings.appId, 'policy-engine');
    assertEquals(settings.location, 'policy-location');
  }

  async testGeminiEnterpriseSettingsPolicyUnset() {
    assertDefined(this.host.getGeminiEnterpriseSettings);
    const settingsObservable = this.host.getGeminiEnterpriseSettings();
    const settings = settingsObservable.getCurrentValue();
    assertUndefined(settings);
  }

  async testGetDisplayMedia() {
    async function waitForFirstFrame(track: MediaStreamVideoTrack):
        Promise<boolean> {
      const processor = new MediaStreamTrackProcessor({track});
      const reader = processor.readable.getReader();

      try {
        const result = await reader.read();
        if (result.done) {
          throw new ApiTestError('Track ended before a frame could be read.');
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
    assertDefined(track);
    assertTrue(await waitForFirstFrame(track));
  }

  async testJournal() {
    assertDefined(this.host.getJournalHost);
    const journalHost = this.host.getJournalHost();
    assertDefined(journalHost);
    journalHost.start(64 * 1024 * 1024, true);
    let snapshot = await journalHost.snapshot(false);
    let lastJournalSize = snapshot.data.byteLength;
    assertTrue(lastJournalSize > 0);
    journalHost.instantEvent(23, 'instant_event', 'some_details');
    snapshot = await journalHost.snapshot(false);
    assertTrue(snapshot.data.byteLength > lastJournalSize);
    lastJournalSize = snapshot.data.byteLength;
    journalHost.clear();
    snapshot = await journalHost.snapshot(false);
    assertTrue(snapshot.data.byteLength < lastJournalSize);
    lastJournalSize = snapshot.data.byteLength;
    journalHost.beginAsyncEvent(10, 23, 'async_event', 'some_details');
    journalHost.endAsyncEvent(10, 'some_details_end');
    snapshot = await journalHost.snapshot(false);
    assertTrue(snapshot.data.byteLength > lastJournalSize);
    lastJournalSize = snapshot.data.byteLength;
    journalHost.stop();
  }

  async testStopMicrophone() {
    const stopMicrophonePromise = Promise.withResolvers<void>();
    this.client.onStopMicrophone = () => {
      stopMicrophonePromise.resolve();
    };

    await this.advanceToNextStep();
    await waitFor(stopMicrophonePromise.promise);
  }

  async testSetSyntheticExperimentState() {
    assertDefined(this.host.setSyntheticExperimentState);
    this.host.setSyntheticExperimentState('TestTrial', 'Enabled');
  }

  async testSetSyntheticExperimentStateMultiProfile() {
    assertDefined(this.host.setSyntheticExperimentState);
    this.host.setSyntheticExperimentState('TestTrial', 'Group1');
    this.host.setSyntheticExperimentState('TestTrial', 'Group2');
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

  async testTabDataUpdateOnUrlChangeForPinnedTab() {
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.pinTabs);

    const tabId = (this.testParams as {tabId: string}).tabId;
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

    const tabId = (this.testParams as {tabId: string}).tabId;
    assertNotEquals(tabId, this.getActiveTabId());

    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());

    await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.some(t => t.tabId === tabId && t.favicon === undefined));

    // Update the favicon.
    await this.advanceToNextStep();

    const tabs = await pinnedTabsUpdates.waitFor(
        (tabs) => tabs.some(t => t.tabId === tabId && t.favicon !== undefined));

    const tabData = tabs.find(t => t.tabId === tabId);
    const blob = await tabData?.favicon?.();
    assertEquals(blob?.type, 'image/bmp');
    assertTrue(checkDefined(blob).size > 0);
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

  async testUserInputSubmittedPromptType() {
    assertDefined(this.host.getMetrics);
    const metrics = this.host.getMetrics();
    assertDefined(metrics);
    assertDefined(metrics.onUserInputSubmitted);
    metrics.onUserInputSubmitted(WebClientMode.TEXT, PromptType.TYPED_TEXT);
  }

  // TODO(crbug.com/454083080): Fix this, it hangs.
  async testCaptureScreenshot() {
    assertDefined(this.host.captureScreenshot);
    const screenshot = await this.host.captureScreenshot?.();
    assertDefined(screenshot);
    assertTrue(screenshot.widthPixels > 0);
    assertTrue(screenshot.heightPixels > 0);
    assertTrue(screenshot.data.byteLength > 0);
    assertEquals(screenshot.mimeType, 'image/jpeg');
  }

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

    if (shouldGetScreenshot) {
      assertDefined(context.viewportScreenshot);
    } else {
      // For platforms where screenshotting fails while minimized, it fails
      // randomly, so we don't assert anything here. This test at least confirms
      // this call does not crash.
    }
  }

  async testHibernateAllOnMemoryPressure() {}
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

class FaviconTest extends ApiTests {
  async testFaviconLoadsWithGetTabById() {
    const fetchBlobForTab = (tabId: string): Observable<Blob|undefined> => {
      assertDefined(this.host.getTabById);
      return mapObservable(this.host.getTabById(tabId), (tabData: TabData) => {
        if (!tabData.favicon) {
          return undefined;
        }
        return tabData.favicon();
      });
    };
    await this.doFaviconTest(fetchBlobForTab);
  }

  async testFaviconLoadsWithGetTabFaviconById() {
    assertDefined(this.host.getTabFaviconById);
    await this.doFaviconTest((id) => this.host.getTabFaviconById!(id));
  }

  shortColor(r: number, g: number, b: number, a: number) {
    function shortHex(v: number) {
      return Math.floor(v / 16).toString(16);
    }
    return `#${shortHex(r)}${shortHex(g)}${shortHex(b)}${shortHex(a)}`;
  }

  async blobToImageData(blob: Blob): Promise<ImageData> {
    const url = URL.createObjectURL(blob);
    const img = new Image();
    await new Promise((resolve, reject) => {
      img.onload = resolve;
      img.onerror = reject;
      img.src = url;
    });

    const canvas = document.createElement('canvas');
    canvas.width = img.width;
    canvas.height = img.height;
    const ctx = canvas.getContext('2d')!;
    ctx.drawImage(img, 0, 0);

    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    URL.revokeObjectURL(url);
    return imageData;
  }

  async getImageColors(image: Blob|undefined): Promise<string|undefined> {
    if (!image) {
      return undefined;
    }
    const imageData = await this.blobToImageData(image);
    const colors = new Set<string>();
    for (let i = 0; i < imageData.data.length; i += 4) {
      const r = imageData.data[i]!;
      const g = imageData.data[i + 1]!;
      const b = imageData.data[i + 2]!;
      if (r > 0 || g > 0 || b > 0) {
        colors.add(this.shortColor(r, g, b, 255));
      }
    }
    const colorArray = Array.from(colors);
    colorArray.sort();
    return colorArray.join(',');
  }

  observeFaviconColorsForTab(tabId: string): Observable<string|undefined> {
    assertDefined(this.host.getTabFaviconById);
    return mapObservable(
        this.host.getTabFaviconById(tabId), async (blob: Blob|undefined) => {
          return this.getImageColors(blob);
        });
  }

  async doFaviconTest(
      fetchFaviconForTab: (tabId: string) => Observable<Blob|undefined>) {
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 2);
    if (!tabs) {
      throw new Error('No tabs');
    }

    for (const tab of tabs) {
      const checkFaviconBlob =
          async(newBlob: Blob|undefined): Promise<boolean|Error> => {
        if (!newBlob) {
          return new Error('No blob');
        }

        const observedColors = await this.getImageColors(newBlob);

        const page1Colors = ['#00ff'];
        const page2Colors = ['#f00f'];

        let expectedColors;
        if (tab.url.indexOf('page.html') !== -1) {
          expectedColors = page1Colors;
        } else {
          expectedColors = page2Colors;
        }
        if (expectedColors.join(',') !== observedColors) {
          return new Error(`Color mismatch for ${tab.url}! Expected: ${
              expectedColors.join(',')} Observed: ${observedColors}`);
        }
        return true;
      };
      await observeSequence(fetchFaviconForTab(tab.tabId))
          .waitFor(f => checkFaviconBlob(f));
    }
  }

  async testFaviconIsUpdated() {
    assertDefined(this.host.getPinnedTabs);
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 1);
    const tab = tabs[0]!;
    const faviconColors =
        observeSequence(this.observeFaviconColorsForTab(tab.tabId));
    await faviconColors.waitFor((colors) => colors === '#00ff');

    // Change the page's favicon to red.
    assertTrue(await this.browser.execJsInTab(tab.tabId, `
      var link = document.querySelector("link[rel~='icon']");
      link.href = "./red.ico";
    `));

    await faviconColors.waitFor((colors) => colors === '#f00f');
  }

  async testFaviconIsRemoved() {
    assertDefined(this.host.getPinnedTabs);
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 1);
    const tab = tabs[0]!;
    const faviconColors =
        observeSequence(this.observeFaviconColorsForTab(tab.tabId));
    await faviconColors.waitFor((colors) => colors === '#00ff');

    // Navigate to a page without a favicon.
    assertTrue(await this.browser.navigateTab(
        tab.tabId,
        new URL(
            '/test_data/page_no_favicon.html',
            this.initData!.embeddedTestServerUrl)
            .href));

    // We should see the generic globe icon. Just assert there is a change.
    await faviconColors.waitFor((colors) => colors !== '#00ff');
  }

  async testTabFaviconObserverLifecycleAndCleanup() {
    assertDefined(this.host.getPinnedTabs);
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 1);
    const tab = tabs[0]!;

    assertDefined(this.host.getTabFaviconById);
    const subscription =
        this.host.getTabFaviconById(tab.tabId).subscribe(() => {});

    await this.advanceToNextStep();

    subscription.unsubscribe();
  }

  async testTabFaviconObserverTabWillClose() {
    assertDefined(this.host.getPinnedTabs);
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 1);
    const tab = tabs[0]!;

    assertDefined(this.host.getTabFaviconById);
    this.host.getTabFaviconById(tab.tabId).subscribe(() => {});
  }

  async testAndroidFaviconUpdatedViaObserver() {
    assertDefined(this.host.getPinnedTabs);
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 1);
    const tab = tabs[0]!;
    const faviconColors =
        observeSequence(this.observeFaviconColorsForTab(tab.tabId));
    await faviconColors.waitFor((colors) => colors === '#00ff');

    await this.advanceToNextStep();

    await faviconColors.waitFor((colors) => colors === '#f00f');
  }

  async testWebClientReadyOnFullLoad() {}
}

class FaviconOmittedTest extends FaviconTest {
  override createWebClient() {
    const client = super.createWebClient();
    (client as GlicWebClient).getClientCapabilities = () => {
      return new Set([ClientCapabilities.IGNORES_TAB_DATA_FAVICONS]);
    };
    return client;
  }

  async testFaviconIsOmittedWithClientCapabilities() {
    assertDefined(this.host.getPinnedTabs);
    const tabs = await observeSequence(this.host.getPinnedTabs!())
                     .waitFor((tabs) => tabs.length === 1);
    const tab = tabs[0]!;

    assertUndefined(tab.favicon);
  }
}

class InvokeClient extends WebClient {
  calls: string[] = [];
  lastInvokeOptions: InvokeOptions|null = null;
  override async notifyPanelWillOpen(
      panelOpeningData: PanelOpeningData&PanelState): Promise<OpenPanelInfo> {
    this.calls.push('notifyPanelWillOpen');
    return super.notifyPanelWillOpen!(panelOpeningData);
  }

  override async invoke(options: InvokeOptions): Promise<void> {
    this.calls.push('invoke');
    this.lastInvokeOptions = options;
  }
}

class InvokeTest extends ApiTests {
  override createWebClient() {
    return new InvokeClient();
  }

  getInvokeClient(): InvokeClient {
    return this.client as InvokeClient;
  }

  async testInvokeWaitsForNotifyPanelWillOpen() {
    const client = this.getInvokeClient();
    await runUntil(() => {
      return client.calls.length === 2;
    });

    assertEquals('notifyPanelWillOpen,invoke', client.calls.join(','));
  }

  async testInvoke() {
    const client = this.getInvokeClient();
    await runUntil(() => {
      return client.lastInvokeOptions !== null;
    });
    assertDefined(client.lastInvokeOptions);
    assertEquals(
        client.lastInvokeOptions!.invocationSource,
        InvocationSource.TOP_CHROME_BUTTON);
  }
}

class TriggeringUpdatesClient extends WebClient {
  triggeringUpdatesSubject = new Subject<ExperimentalTriggeringUpdate>();

  async getExperimentalTriggeringUpdates():
      Promise<Observable2<ExperimentalTriggeringUpdate>> {
    return this.triggeringUpdatesSubject;
  }
}

class TriggeringUpdatesTest extends ApiTests {
  override createWebClient() {
    return new TriggeringUpdatesClient();
  }

  async testGetExperimentalTriggeringUpdates() {
    const client = this.client as TriggeringUpdatesClient;

    // Step 1: Wait for C++ to trigger the request.
    await this.advanceToNextStep();

    // The host should have requested updates.
    await runUntil(
        () => client.triggeringUpdatesSubject.hasActiveSubscription());

    // Push a terminal update to trigger cleanup.
    client.triggeringUpdatesSubject.next({
      type: ExperimentalTriggeringUpdateType.TERMINAL_COMPLETION,
      data: '',
    });
    client.triggeringUpdatesSubject.complete();

    // Verify that the subscriber auto-unsubscribed on the client side.
    await runUntil(
        () => !client.triggeringUpdatesSubject.hasActiveSubscription());
  }

  async testGetExperimentalTriggeringUpdatesError() {
    const client = this.client as TriggeringUpdatesClient;

    // Step 1: Wait for C++ to trigger the request.
    await this.advanceToNextStep();

    // The host should have requested updates.
    await runUntil(
        () => client.triggeringUpdatesSubject.hasActiveSubscription());

    // Trigger error.
    client.triggeringUpdatesSubject.error(new Error('Error'));

    // Verify that the subscriber auto-unsubscribed on the client side.
    await runUntil(
        () => !client.triggeringUpdatesSubject.hasActiveSubscription());
  }
}

type InitFailureType = 'error'|'timeout'|'none'|'reloadAfterInitialize'|
    'navigateToSorryPageBeforeInitialize'|'navigateToSorryPageAfterInitialize';

class WebClientThatFailsInitialize extends WebClient {
  constructor(private failWith: InitFailureType = 'error') {
    super();
  }

  override initialize(glicBrowserHost: any): Promise<void> {
    if (this.failWith === 'error') {
      return Promise.reject(
          new ApiTestError('WebClientThatFailsInitialize.initialize'));
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

  override async setUpClient() {}

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
    if (this.getTestParams().failWith ===
        'navigateToSorryPageBeforeInitialize') {
      this.deferredSetUpClient();
    } else if (this.getTestParams().failWith === 'none') {
      await super.setUpClient();
    }
  }

  async testSorryPageAfterInitialize() {
    if (this.getTestParams().failWith ===
        'navigateToSorryPageAfterInitialize') {
      this.deferredSetUpClient();
    } else if (this.getTestParams().failWith === 'none') {
      await super.setUpClient();
    }
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

  async testInitializeFails() {
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
    await panelOpenState.waitForValue(true);
    await this.host.closePanel!();
    await panelOpenState.waitForValue(false);
    await this.advanceToNextStep();
    openSignal.resolve();
    await panelOpenState.waitForValue(true);
  }
}

class SkillsApiTests extends ApiTests {
  async testGetSkillSuccess() {
    assertDefined(this.host.skills);
    const skillsApi = await observeSequence(this.host.skills()).next();
    assertDefined(skillsApi);
    assertDefined(skillsApi.getSkillPreviews);
    assertDefined(skillsApi.getSkill);
    const skillPreviewsSequence = observeSequence(skillsApi.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    const targetSkill = skills.find(s => s.name === 'test_skill_1');
    assertDefined(targetSkill);
    const actualSkill = await skillsApi.getSkill(targetSkill.id);
    assertDefined(actualSkill);
    assertEquals(actualSkill.preview.id, targetSkill.id);
    assertEquals(actualSkill.preview.name, 'test_skill_1');
    assertEquals(actualSkill.preview.icon, 'test_icon_1');
    assertEquals(actualSkill.prompt, 'test_prompt_1');
    assertEquals(actualSkill.sourceSkillId, 'source_id_1');
  }

  async testGetSkillPreviewsSuccess() {
    assertDefined(this.host.getSkillPreviews);
    assertDefined(this.host.getSkill);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    const skill1 = skills.find(s => s.name === 'test_skill_1');
    assertDefined(skill1);
    assertEquals('test_icon_1', skill1.icon);
    assertTrue(skill1.creationTime instanceof Date);
    const actualSkill1 = await this.host.getSkill(skill1.id);
    assertDefined(actualSkill1);
    assertEquals(actualSkill1.sourceSkillId, 'source_id_1');
    assertEquals(
        actualSkill1.preview.creationTime?.getTime(),
        skill1.creationTime.getTime());
    const skill2 = skills.find(s => s.name === 'test_skill_2');
    assertDefined(skill2);
    assertEquals('test_icon_2', skill2.icon);
    assertTrue(skill2.creationTime instanceof Date);
    const actualSkill2 = await this.host.getSkill(skill2.id);
    assertDefined(actualSkill2);
    assertEquals(actualSkill2.sourceSkillId, 'source_id_2');
    assertEquals(
        actualSkill2.preview.creationTime?.getTime(),
        skill2.creationTime.getTime());
  }

  async testGetSkillDisabled() {
    // Check that skills are disabled via the new API
    assertDefined(this.host.skills);
    assertUndefined(await observeSequence(this.host.skills()).next());

    // API should be gone when disabled.
    assertUndefined(this.host.getSkill);
    assertUndefined(this.host.createSkill);
    assertUndefined(this.host.updateSkill);
    assertUndefined(this.host.showManageSkillsUi);
    assertUndefined(this.host.showBrowseSkillsUi);
    assertUndefined(this.host.recordSkillsWebClientEvent);
    assertUndefined(this.host.getSkillPreviews);
    assertUndefined(this.host.getSkillToInvoke);
  }

  async testSendingContextualSkillsToGlic() {
    assertDefined(this.host.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    let skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    let user_skill_1 = skills.find(s => s.name === 'user_skill_1');
    assertDefined(user_skill_1);
    let user_skill_2 = skills.find(s => s.name === 'user_skill_2');
    assertDefined(user_skill_2);
    await this.advanceToNextStep();

    skills = await skillPreviewsSequence.waitFor(s => s.length === 4);
    const contextual_skill_1 =
        skills.find(s => s.id === 'contextual_skill_id_1');
    assertDefined(contextual_skill_1);
    assertEquals('contextual_skill_1', contextual_skill_1.name);
    assertEquals(
        'contextual_skill_description_1', contextual_skill_1.description);
    const contextual_skill_2 =
        skills.find(s => s.id === 'contextual_skill_id_2');
    assertDefined(contextual_skill_2);
    assertEquals('contextual_skill_2', contextual_skill_2.name);
    assertEquals(
        'contextual_skill_description_2', contextual_skill_2.description);
    user_skill_1 = skills.find(s => s.name === 'user_skill_1');
    assertDefined(user_skill_1);
    user_skill_2 = skills.find(s => s.name === 'user_skill_2');
    assertDefined(user_skill_2);
    assertEquals(true, contextual_skill_1.isContextual);
    assertEquals(true, contextual_skill_2.isContextual);
    assertEquals(false, user_skill_1.isContextual);
    assertEquals(false, user_skill_2.isContextual);
    await this.advanceToNextStep();

    skills = await skillPreviewsSequence.waitFor(s => s.length === 3);
    const contextual_skill_3 =
        skills.find(s => s.id === 'contextual_skill_id_3');
    assertDefined(contextual_skill_3);
    assertEquals('contextual_skill_3', contextual_skill_3.name);
    assertEquals(
        'contextual_skill_description_3', contextual_skill_3.description);
    user_skill_1 = skills.find(s => s.name === 'user_skill_1');
    assertDefined(user_skill_1);
    user_skill_2 = skills.find(s => s.name === 'user_skill_2');
    assertDefined(user_skill_2);
    assertEquals(true, contextual_skill_3.isContextual);
    assertEquals(false, user_skill_1.isContextual);
    assertEquals(false, user_skill_2.isContextual);
  }

  async testSendingPendingContextualSkillsToGlic() {
    assertDefined(this.host.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 1);
    const contextual_skill_1 =
        skills.find(s => s.id === 'contextual_skill_id_1');
    assertDefined(contextual_skill_1);
    assertEquals('contextual_skill_1', contextual_skill_1.name);
    assertEquals(
        'contextual_skill_description_1', contextual_skill_1.description);
    assertEquals(true, contextual_skill_1.isContextual);
  }

  async testChangingActiveTabClearsPendingContextualSkills() {
    assertDefined(this.host.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.next();
    assertEquals(0, skills.length);
  }
}

// TODO(b/546606964): enable these tests on android.
class SkillsDesktopOnlyApiTests extends SkillsApiTests {
  async testSkillsEnabledToggledAtRuntime() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    // 1. Initially disabled.
    assertUndefined(await skillsSequence.next());

    // 2. Enable skills pref at runtime.
    await this.advanceToNextStep();
    const enabledSkills = await skillsSequence.next();
    assertDefined(enabledSkills);

    // 3. Disable skills pref at runtime.
    await this.advanceToNextStep();
    assertUndefined(await skillsSequence.next());
  }

  async testContextualSkillsRetainedWhenStartingPrefDisabled() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    // Initially disabled.
    assertUndefined(await skillsSequence.next());

    // Step 1: Enable skills pref at runtime and verify cached contextual skills
    // are received.
    await this.advanceToNextStep();
    const enabledSkills = await skillsSequence.next();
    assertDefined(enabledSkills);
    assertDefined(enabledSkills.getSkillPreviews);

    const previewsSeq = observeSequence(enabledSkills.getSkillPreviews());
    const previews = await previewsSeq.waitFor(s => s.length === 1);
    assertEquals('contextual_skill_id_1', previews[0]?.id);
    assertEquals('contextual_skill_1', previews[0]?.name);
  }

  async testSkillsEnabledState() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    const skills = await skillsSequence.next();
    assertDefined(skills);

    // Call when enabled
    assertDefined(skills.getSkill);
    await assertRejects(skills.getSkill('non-existent-id'));

    // Get a valid skill ID from getSkillPreviews.
    assertDefined(skills.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(skills.getSkillPreviews());
    const skillPreviews =
        await skillPreviewsSequence.waitFor(s => s.length === 1);
    const skillId = skillPreviews[0]!.id;

    // Verify that both the new API and deprecated API succeed when skills are
    // enabled.
    assertDefined(skills.recordSkillsWebClientEvent);
    skills.recordSkillsWebClientEvent(SkillsWebClientEvent.OPENED_MENU);

    assertDefined(skills.getSkill);
    const skillFromNewApi = await skills.getSkill(skillId);
    assertDefined(skillFromNewApi);
    assertEquals('source_id_1', skillFromNewApi.sourceSkillId);

    assertDefined(this.host.getSkill);
    const skillFromDeprecatedApi = await this.host.getSkill(skillId);
    assertDefined(skillFromDeprecatedApi);
    assertEquals('source_id_1', skillFromDeprecatedApi.sourceSkillId);
    assertDefined(this.host.getSkillToInvoke);

    await this.advanceToNextStep();
    assertUndefined(await skillsSequence.next());

    // When skills are disabled, API methods that return a Promise should reject
    // with an error, both when calling via a saved reference to
    // GlicBrowserSkills (new API)...
    assertDefined(skills.recordSkillsWebClientEvent);
    skills.recordSkillsWebClientEvent(SkillsWebClientEvent.OPENED_MENU);
    assertDefined(skills.getSkill);
    await assertRejects(skills.getSkill(skillId));
    assertDefined(skills.createSkill);
    await assertRejects(skills.createSkill({prompt: 'test'}));
    assertDefined(skills.updateSkill);
    await assertRejects(skills.updateSkill({id: skillId}));

    // ...and when calling via GlicBrowserHost (deprecated API).
    assertDefined(this.host.recordSkillsWebClientEvent);
    this.host.recordSkillsWebClientEvent(SkillsWebClientEvent.OPENED_MENU);
    assertDefined(this.host.getSkill);
    await assertRejects(this.host.getSkill!(skillId));
    assertDefined(this.host.createSkill);
    await assertRejects(this.host.createSkill!({prompt: 'test'}));
    assertDefined(this.host.updateSkill);
    await assertRejects(this.host.updateSkill!({id: skillId}));

    // Synchronous void functions that couldn't throw an error previously must
    // fail silently without throwing an error, both on GlicBrowserSkills (new
    // API) and on GlicBrowserHost (deprecated API).
    assertDefined(skills.showManageSkillsUi);
    skills.showManageSkillsUi!();
    assertDefined(skills.showBrowseSkillsUi);
    skills.showBrowseSkillsUi!();
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi!();
    assertDefined(this.host.showBrowseSkillsUi);
    this.host.showBrowseSkillsUi!();

    // Advance to next step (re-enable skills) and verify skills observable
    // emits a new instance.
    await this.advanceToNextStep();
    const reenabledSkills = await skillsSequence.next();
    assertDefined(reenabledSkills);
    assertDefined(reenabledSkills.getSkill);
    const reenabledSkill = await reenabledSkills.getSkill(skillId);
    assertDefined(reenabledSkill);
    assertEquals('source_id_1', reenabledSkill.sourceSkillId);
  }

  async testCreateSkillAndDisable() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    const skills = await skillsSequence.next();
    assertDefined(skills);
    assertDefined(skills.createSkill);

    const request = {
      id: 'id',
      name: 'name',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.FIRST_PARTY,
    };
    await skills.createSkill(request);

    // Advance to step 2 where C++ disables skills and closes the dialog.
    await this.advanceToNextStep();
    assertUndefined(await skillsSequence.next());
    await assertRejects(skills.createSkill(request));
  }

  async testShowManageSkillsUi() {
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi();
  }

  async testShowBrowseSkillsUi() {
    assertDefined(this.host.showBrowseSkillsUi);
    this.host.showBrowseSkillsUi();
  }

  async testDisplaySkillInDialogSuccess() {
    assertDefined(this.host.createSkill);
    const request = {
      id: 'id',
      name: 'name',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.FIRST_PARTY,
    };
    this.host.createSkill(request);
  }

  async testShowManageSkillsUiNoWindow() {
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi();
  }

  async testDisableDragResize() {
    assertDefined(this.host.enableDragResize);
    await this.host.enableDragResize(false);
  }

  async testSetMinimumWidgetSize() {
    assertDefined(this.host.setMinimumWidgetSize);
    const minSize = {width: 200, height: 100};
    await this.host.setMinimumWidgetSize(minSize.width, minSize.height);
    await this.advanceToNextStep(minSize);
  }

  async testManualResizeChanged() {
    assertDefined(this.host.isManuallyResizing);
    const seq = observeSequence(this.host.isManuallyResizing());
    await seq.waitForValue(true);

    await this.advanceToNextStep();
    await seq.waitForValue(false);
    seq.unsubscribe();
  }

  async testResizeWindowTooSmall() {
    assertDefined(this.host.resizeWindow);
    await this.host.resizeWindow(0, 0);
  }

  async testResizeWindowTooLarge() {
    assertDefined(this.host.resizeWindow);
    await this.host.resizeWindow(20000, 20000);
  }

  async testResizeWindowWithinBounds() {
    assertDefined(this.host.resizeWindow);
    assertDefined(this.testParams);
    await this.host.resizeWindow(this.testParams.width, this.testParams.height);
  }

  async testCreateSkillNoWindow() {
    assertDefined(this.host.createSkill);
    const request = {
      id: 'id',
      name: 'name',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.FIRST_PARTY,
    };
    this.host.createSkill(request);
  }
}

class ContextCapturingClient extends WebClient {
  capturedContext: AdditionalContext[] = [];

  override async initialize(glicBrowserHost: GlicBrowserHost): Promise<void> {
    await super.initialize(glicBrowserHost);
    glicBrowserHost.getAdditionalContext!
        ().subscribe((context: AdditionalContext) => {
          this.capturedContext.push(context);
        });
  }
}

class AdditionalContextQueuedTest extends ApiTestFixtureBase {
  override createWebClient(): WebClient {
    return new ContextCapturingClient();
  }

  async testAdditionalContextQueued() {
    const client = this.client as ContextCapturingClient;
    await runUntil(() => client.capturedContext.length > 0);
    const context = client.capturedContext[0];
    assertDefined(context);
    assertEquals(context.name, 'queued part');
    assertEquals(context.parts.length, 1);
    const part1 = context.parts[0]!;
    assertDefined(part1.data);
    assertEquals(part1.data!.type, 'text/plain');
    const data1 = new Uint8Array(await part1.data!.arrayBuffer());
    assertEquals(new TextDecoder().decode(data1), 'queued');
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

  async testEnableDragResize() {
    assertDefined(this.host.enableDragResize);
    await this.host.enableDragResize(true);
  }
}

class ScreenshotTests extends ApiTestFixtureBase {
  async testCaptureAndUploadEncryptedScreenshot() {
    await runUntil(() => this.client.lastUploadedScreenshot !== null);
    assertDefined(this.client.lastUploadedScreenshot);
    assertTrue(this.client.lastUploadedScreenshot!.data.byteLength > 0);
    assertEquals(
        this.client.lastUploadedScreenshot!.encryptionScheme,
        ScreenshotEncryptionScheme.RFC8291);
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
    const client = this.client as WebClientThatOpensOnce;
    await runUntil(() => client.notifyPanelWillOpenCallCount > 0);
    assertEquals(client.notifyPanelWillOpenCallCount, 1);
    client.notifyPanelWillOpenCallCount = 0;
    await this.advanceToNextStep();
    await runUntil(() => client.notifyPanelWillOpenCallCount > 0);
    assertEquals(client.notifyPanelWillOpenCallCount, 1);
  }
}

const TEST_FIXTURES: Array<typeof ApiTestFixtureBase> = [
  ApiTests,
  DaisyChainApiTests,
  AdditionalContextQueuedTest,
  FaviconTest,
  FaviconOmittedTest,
  InvokeTest,
  ApiTestFailsToInitialize,
  TriggeringUpdatesTest,
  ScreenshotTests,
  NotifyPanelWillOpenTest,
  SkillsApiTests,
];

// TODO(b/546606964): enable these tests on android.
if (!navigator.userAgent.includes('Android')) {
  TEST_FIXTURES.push(SkillsDesktopOnlyApiTests, InitiallyNotResizableTest);
}

testMain(TEST_FIXTURES);
