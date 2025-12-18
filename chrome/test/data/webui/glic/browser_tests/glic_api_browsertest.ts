// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {CaptureRegionErrorReason, ClientView, HostCapability, MetricUserInputReactionType, PanelStateKind, ResponseStopCause, ScrollToErrorReason, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {CaptureRegionResult, FocusedTabData, GetPinCandidatesOptions, GlicBrowserHost, OpenPanelInfo, PageMetadata, PanelOpeningData, ScrollToError, TabData, UserProfileInfo, ViewChangeRequest, ZeroStateSuggestionsV2} from '/glic/glic_api/glic_api.js';

import {ApiTestError, ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertNotEquals, assertRejects, assertTrue, assertUndefined, checkDefined, mapObservable, observeSequence, readStream, runUntil, sleep, testMain, waitFor, WebClient} from './browser_test_base.js';
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

  async testDoNothing() {}

  // This test should fail even if the ApiTestError is captured in a try-catch
  // block.
  async testFailureForCapturedApiTestError() {
    try {
      throw new ApiTestError('Non-throwing test error');
    } catch (e) {
    }
  }

  async testRequestHeader() {
    const rpcUrls: string[] = this.testParams.rpcUrls;
    await Promise.all(rpcUrls.map(url => fetch(url)));
  }

  async testCreateTab() {
    assertDefined(this.host.createTab);
    // Open a tab pointing to test.html.
    const url = location.href;
    const data = await this.host.createTab(url, {openInBackground: false});
    assertEquals(data.url, url);
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
    this.host.setAudioDucking(true);
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();

    await this.advanceToNextStep();
    this.host.setAudioDucking(false);
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

    // TODO(wry): Chrome switches tabs correctly, but focus is not updating.
    // The following code should work:

    // await observeSequence(this.host.getFocusedTabStateV2()).waitFor(update
    // => {
    //   return update.hasFocus?.tabData?.url?.includes('chromium.org') ??
    //   false;
    // });
  }

  async testCreateTabFailsIfNotActive() {
    assertDefined(this.host.closePanel);
    assertDefined(this.host.createTab);
    await this.closePanelAndWaitUntilInactive();
    this.assertCreateTabFails(location.href);
  }

  async testOpenGlicSettingsPage() {
    assertDefined(this.host.openGlicSettingsPage);
    this.host.openGlicSettingsPage();
    // There is a problem with InProcessBrowserTest::QuitBrowsers(). Opening a
    // browser at the same time as exiting a test results in QuitBrowsers()
    // never exiting. This sleep avoids this problem.
    await sleep(500);
  }

  async testOpenPasswordManagerSettingsPage() {
    assertDefined(this.host.openPasswordManagerSettingsPage);
    this.host.openPasswordManagerSettingsPage();
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

  async testMultiplePanelsDetachedAndFloating() {
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


  async testClosePanel() {
    assertDefined(this.host.closePanel);

    // Close the panel, and verify notifyPanelWasClosed is called.
    const closedPromise = Promise.withResolvers<void>();
    this.client.onNotifyPanelWasClosed = closedPromise.resolve;
    await this.host.closePanel();
    await waitFor(closedPromise.promise);
  }

  async testErrorShownOnMojoPipeError() {}

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

  async testIsBrowserOpen() {
    assertDefined(this.host.isBrowserOpen);
    // This test closes the browser, so we need to detach the side panel to
    // avoid closing glic.
    await this.detachIfInMultiInstance();
    const isBrowserOpen = observeSequence(this.host.isBrowserOpen());
    assertTrue(await isBrowserOpen.next());
    // Close the browser.
    await this.advanceToNextStep();
    assertTrue(!await isBrowserOpen.next());
  }

  async testEnableDragResize() {
    assertDefined(this.host.enableDragResize);

    await this.host.enableDragResize(true);
  }

  async testDisableDragResize() {
    assertDefined(this.host.enableDragResize);

    await this.host.enableDragResize(false);
  }

  async testGetZeroStateSuggestionsForFocusedTabApi() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsForFocusedTabFailsWhenHidden() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsApi() {
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);
    assertEquals(false, suggestions.isPending);
  }

  async testGetZeroStateSuggestionsMultipleNavigations() {
    // Initial state.
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);
    assertEquals(false, suggestions.isPending);

    // After a second navigation occurs.
    await this.advanceToNextStep();

    // Should first get a pending state.
    const suggestions2 = await sequence.next();
    assertDefined(suggestions2);
    // We don't care about the suggestions here.
    assertEquals(true, suggestions2.isPending);

    // Should later get the actual suggestions.
    const suggestions3 = await sequence.next();
    assertDefined(suggestions3);
    assertEquals(3, suggestions3.suggestions.length);
    assertEquals(false, suggestions3.isPending);
  }

  async testGetZeroStateSuggestionsFailsWhenHidden() {
    // Initial state.
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);

    // Close panel.
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();

    // After next navigation in focused tab occurs.
    await this.advanceToNextStep();
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

  async testGetFocusedTabStateV2WithNavigation() {
    // Initial state.
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertDefined(focus.hasFocus);
    assertEquals(
        new URL(focus.hasFocus.tabData.url).pathname,
        '/glic/browser_tests/test.html', `url=${focus.hasFocus.tabData.url}`);
    assertFalse(!!focus.hasNoFocus);

    // After a second navigation occurs.
    await this.advanceToNextStep();
    const focus2 = await sequence.next();
    assertDefined(focus2.hasFocus);
    assertEquals(
        new URL(focus2.hasFocus.tabData.url).pathname,
        '/scrollable_page_with_content.html',
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
    assertDefined(focus3.hasFocus);
    assertEquals(
        new URL(focus3.hasFocus.tabData.url).pathname,
        '/glic/browser_tests/test.html', `url=${focus3.hasFocus.tabData.url}`);
    assertFalse(!!focus3.hasNoFocus);
  }

  async testGetFocusedTabStateV2WithNavigationWhenInactive() {
    // Initial state.
    assertDefined(this.host.getFocusedTabStateV2);
    await this.closePanelAndWaitUntilInactive();
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    const focus = await sequence.next();
    assertDefined(focus.hasFocus);
    assertEquals(
        new URL(focus.hasFocus.tabData.url).pathname,
        '/glic/browser_tests/test.html', `url=${focus.hasFocus.tabData.url}`);
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
    assertDefined(focus2.hasFocus);
    assertEquals(
        new URL(focus2.hasFocus.tabData.url).pathname,
        '/glic/browser_tests/test.html', `url=${focus2.hasFocus.tabData.url}`);
    assertFalse(!!focus2.hasNoFocus);
  }

  async testSingleFocusedTabUpdatesOnTabEvents() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    // Check events from first tab.
    {
      const focus = await sequence.next();
      assertDefined(
          !!focus.hasFocus,
          `#1: should have a focused tab; FocusedTabData=${
              JSON.stringify(focus)}`);
      assertEquals(
          new URL(focus.hasFocus?.tabData.url).pathname,
          '/glic/browser_tests/test.html',
          `#1: Unexpected URL; FocusedTabData=${JSON.stringify(focus)}`);
      assertTrue(
          sequence.isEmpty(), '#1: Spurious updates after first tab opened');
    }

    // After a navigation occurs in the first tab.
    {
      await this.advanceToNextStep();
      const focus = await sequence.next();
      assertDefined(
          !!focus.hasFocus,
          `#2: should have a focused tab; FocusedTabData=${
              JSON.stringify(focus)}`);
      assertEquals(
          new URL(focus.hasFocus?.tabData.url).pathname,
          '/scrollable_page_with_content.html',
          `#2: Unexpected URL; FocusedTabData=${JSON.stringify(focus)}`);
      assertTrue(
          sequence.isEmpty(), '#2: Spurious updates after first tab navigated');
    }

    // A new tab is opened and navigated.
    {
      await this.advanceToNextStep();
      const focus = await sequence.next();
      assertDefined(
          !!focus.hasFocus,
          `#3: should have a focused tab; FocusedTabData=${
              JSON.stringify(focus)}`);
      assertEquals(
          new URL(focus.hasFocus?.tabData.url).pathname,
          '/glic/browser_tests/test.html',
          `#3: Unexpected URL; FocusedTabData=${JSON.stringify(focus)}`);
      assertTrue(
          sequence.isEmpty(), '#3: Spurious updates after a new tab opened');
    }
  }

  async testGetFocusedTabStateV2BrowserClosed() {
    assertDefined(this.host.getFocusedTabStateV2);
    const sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    // Ignore the initial focus.
    await sequence.next();
    const focus = await sequence.next();
    assertFalse(!!focus.hasFocus);
    assertDefined(focus.hasNoFocus);
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

  async testPermissionAccess() {
    assertDefined(this.host.getMicrophonePermissionState);
    assertDefined(this.host.getLocationPermissionState);
    assertDefined(this.host.getTabContextPermissionState);

    const microphoneState =
        observeSequence<boolean>(this.host.getMicrophonePermissionState());
    const locationState =
        observeSequence<boolean>(this.host.getLocationPermissionState());
    const tabContextState =
        observeSequence<boolean>(this.host.getTabContextPermissionState());

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
    const defaultTabContextState =
        observeSequence(this.host.getDefaultTabContextPermissionState());
    assertTrue(await defaultTabContextState.next() as boolean);
    assertDefined(this.host.getPinnedTabs);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());

    // The active tab should be automatically pinned on bind.
    const pinnedTabs =
        await pinnedTabsUpdates.waitFor(tabs => tabs.length === 1);
    const activeTabId = this.getActiveTabId();
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

  async testGetOsHotkeyState() {
    assertDefined(this.host.getOsHotkeyState);
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
    assertDefined(this.host.getClosedCaptioningSetting);
    assertDefined(this.host.setClosedCaptioningSetting);
    const closedCaptioningState =
        observeSequence(this.host.getClosedCaptioningSetting());
    assertFalse(await closedCaptioningState.next());
    await this.host.setClosedCaptioningSetting(true);
    assertTrue(await closedCaptioningState.next());
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

  async testGetUserProfileInfo() {
    assertDefined(this.host.getUserProfileInfo);
    const profileInfo = await this.host.getUserProfileInfo();

    assertEquals('', profileInfo.displayName);
    assertEquals('glic-test@example.com', profileInfo.email);
    assertEquals('', profileInfo.givenName);
    assertEquals(false, profileInfo.isManaged!);
    assertTrue((profileInfo.localProfileName?.length ?? 0) > 0);
    // Can be 'Your Chrome' or 'Your Chromium'.
    assertEquals('Your C', profileInfo.localProfileName?.substring(0, 6));
  }

  async testGetUserProfileInfoDoesNotDeferWhenInactive() {
    assertDefined(this.host.getUserProfileInfo);
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    const profileInfo: UserProfileInfo = await this.host.getUserProfileInfo();
    assertEquals('glic-test@example.com', profileInfo.email);
    // Can be 'Your Chrome' or 'Your Chromium'.
    assertEquals('Your C', profileInfo.localProfileName?.substring(0, 6));
  }

  async testRefreshSignInCookies() {
    assertDefined(this.host.refreshSignInCookies);

    await this.host.refreshSignInCookies();
  }

  async testSignInPauseState() {
    assertDefined(this.host.getUserProfileInfo);
    const profileInfo = await this.host.getUserProfileInfo();

    assertEquals('', profileInfo.displayName);
    assertEquals('glic-test@example.com', profileInfo.email);
    assertEquals('', profileInfo.givenName);
    assertEquals(false, profileInfo.isManaged!);
    assertTrue((profileInfo.localProfileName?.length ?? 0) > 0);
  }

  async testSetContextAccessIndicator() {
    assertDefined(this.host.setContextAccessIndicator);

    await this.host.setContextAccessIndicator(true);
  }

  async testSetAudioDucking() {
    assertDefined(this.host.setAudioDucking);

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

  async testScrollToFindsText() {
    assertDefined(this.host.scrollTo);
    assertDefined(this.host.setTabContextPermissionState);
    await this.host.setTabContextPermissionState(true);
    await this.host.scrollTo({
      selector: {exactText: {text: 'Test Page'}},
      highlight: true,
      documentId: this.testParams.documentId,
    });
  }

  async testScrollToFindsTextNoTabContextPermission() {
    assertDefined(this.host.scrollTo);
    try {
      await this.host.scrollTo({
        selector: {exactText: {text: 'Abracadabra'}},
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
        selector: {exactText: {text: 'Abracadabra'}},
        highlight: true,
        documentId: this.testParams.documentId,
      });
    } catch (e) {
      assertEquals(
          ScrollToErrorReason.NOT_SUPPORTED, (e as ScrollToError).reason);
      return;
    }
    assertTrue(false, 'scrollTo should have thrown an error');
  }

  async testScrollToNoMatchFound() {
    assertDefined(this.host.scrollTo);
    assertDefined(this.host.setTabContextPermissionState);
    await this.host.setTabContextPermissionState(true);
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

  async testSetSyntheticExperimentState() {
    assertDefined(this.host.setSyntheticExperimentState);
    this.host.setSyntheticExperimentState('TestTrial', 'Enabled');
  }

  async testSetSyntheticExperimentStateMultiProfile() {
    assertDefined(this.host.setSyntheticExperimentState);
    this.host.setSyntheticExperimentState('TestTrial', 'Group1');
    this.host.setSyntheticExperimentState('TestTrial', 'Group2');
  }

  async testSetWindowDraggableAreas() {
    const draggableAreas = [{x: 10, y: 20, width: 30, height: 40}];
    assertDefined(this.host.setWindowDraggableAreas);
    await this.host.setWindowDraggableAreas(
        draggableAreas,
    );
    await this.advanceToNextStep(draggableAreas);
  }

  async testSetWindowDraggableAreasDefault() {
    assertDefined(this.host.setWindowDraggableAreas);
    await this.host.setWindowDraggableAreas([]);
  }

  async testSetMinimumWidgetSize() {
    assertDefined(this.host.setMinimumWidgetSize);
    const minSize = {width: 200, height: 100};
    await this.host.setMinimumWidgetSize(minSize.width, minSize.height);
    await this.advanceToNextStep(minSize);
  }

  async testManualResizeChanged() {
    assertDefined(this.host.isManuallyResizing);
    await observeSequence(this.host.isManuallyResizing()).waitForValue(true);

    await this.advanceToNextStep();
    await observeSequence(this.host.isManuallyResizing()).waitForValue(false);
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

  async testCallingApiWhileHiddenRecordsMetrics() {
    assertDefined(this.host.createTab);
    await this.advanceToNextStep();
    await runUntil(() => document.visibilityState === 'hidden');
    try {
      await this.host.createTab(
          'https://www.google.com', {openInBackground: false});
    } catch {
    }
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
  }

  async testPinTabsFailsWhenDoesnotExist() {
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

  async testPinTabsStatePersistWhenClosePanelAndReopen() {
    assertDefined(this.host.closePanel);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);

    const tabId = this.testParams.tabId;
    const activeTabId = this.getActiveTabId();

    assertTrue(await this.host.pinTabs([activeTabId, tabId]));
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    await this.host.closePanel();

    // Open glic window again.
    await this.advanceToNextStep();

    assertEquals(this.host.getPinnedTabs().getCurrentValue()?.length, 2);
  }

  async testPinTabsStatePersistWhenClientRestarts() {
    const isFirstRun: boolean = this.testParams.isFirstRun;

    if (isFirstRun) {
      assertDefined(this.host.pinTabs);
      assertDefined(this.host.getPinnedTabs);

      const tabId = this.testParams.tabId;
      const activeTabId = this.getActiveTabId();

      assertTrue(await this.host.pinTabs([activeTabId, tabId]));
      const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
      await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);
    } else {
      assertEquals(this.host.getPinnedTabs?.().getCurrentValue()?.length, 2);
    }
  }

  async testPinTabsFailsWhenIncognitoWindow() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);

    assertFalse(await this.host.pinTabs([this.testParams.incognitoTabId]));

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testUnpinTabsFailsWhenNotPinned() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinTabs);

    const tabId = this.testParams.tabId;
    const tabId2 = this.getActiveTabId();
    // Pin both tabs.
    assertTrue(await this.host.pinTabs([tabId2, tabId]));

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    // Unpin tabId.
    assertTrue(await this.host.unpinTabs([tabId]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 1);

    // Unpinning a tab that is not pinned should fail.
    assertFalse(await this.host.unpinTabs([tabId, tabId2]));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testUnpinAllTabs() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.unpinAllTabs);

    const tabId = this.testParams.tabId;
    const tabId2 = this.getActiveTabId();

    // Pin both tabs.
    assertTrue(await this.host.pinTabs([tabId2, tabId]));

    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    // Unpin all tabs.
    this.host.unpinAllTabs();
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
  }

  async testPinTabsHaveNoEffectOnFocusedTab() {
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.unpinAllTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.getFocusedTabStateV2);

    await this.host.setTabContextPermissionState(true);
    const tabId: string = this.testParams.tabId;

    const focusSequence = observeSequence(this.host.getFocusedTabStateV2());
    const focus = await focusSequence.next();
    const focusedTabId = checkDefined(focus?.hasFocus?.tabData.tabId);
    // Make sure tabId is not the focused tab.
    assertNotEquals(tabId, focusedTabId);

    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 && tabs.at(0)?.tabId === tabId);

    assertTrue(focusSequence.isEmpty());

    this.host.unpinAllTabs();
    pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);

    assertTrue(focusSequence.isEmpty());
  }

  // Tests that tabs which navigate are unpinned if the glic window is closed.
  async testUnpinTabsThatNavigateInBackground() {
    assertDefined(this.host.getPinCandidates);
    assertDefined(this.host.pinTabs);
    assertDefined(this.host.getPinnedTabs);
    assertDefined(this.host.closePanel);

    // Pin all tabs.
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    const candidates = await observeSequence(this.host.getPinCandidates({
                         maxCandidates: 3,
                       })).next();
    assertEquals(candidates.length, 2);
    assertTrue(await this.host.pinTabs(candidates.map(c => c.tabData.tabId)));
    await pinnedTabsUpdates.waitFor((tabs) => tabs.length === 2);

    await this.host.closePanel();

    // Open glic window again.
    await this.advanceToNextStep();

    // Wait for pin updates. We should see one fewer pinned tab, and one
    // navigated tab.
    // Note: pinned tab updates will see the navigation just before the tab is
    // unpinned.
    await pinnedTabsUpdates.waitFor(
        tabs => tabs.map(t => new URL(t.url).search).sort().join(',') ===
            '?changedOne');
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
    pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 && tabs.some((t) => t.tabId === tabId));

    const result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(result.tabData.tabId, tabId);
  }

  async testGetContextFromTabFailDifferentlyBasedOnPermission() {
    assertDefined(this.host.getContextFromTab);
    // For unfocused unpinned tabs, getTabContext call fail with different error
    // messages based on Context sharing permission state.

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

    await this.host.pinTabs([tabId]);
    const pinnedTabsUpdates = observeSequence(this.host.getPinnedTabs());
    pinnedTabsUpdates.waitFor(
        (tabs) => tabs.length === 1 && tabs.at(0)?.tabId === tabId);

    const result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(result.tabData.tabId, tabId);

    await this.host.unpinTabs([tabId]);
    pinnedTabsUpdates.waitFor((tabs) => tabs.length === 0);
    await assertRejects(this.host.getContextFromTab(tabId, {}), {
      withErrorMessage: 'tabContext failed: permission denied:' +
          ' context permission not enabled',
    });
  }

  async testGetContextFromTabFailsIfDoesNotExist() {
    assertDefined(this.host.getContextFromTab);

    await assertRejects(
        this.host.getContextFromTab('not-exist', {}),
        {withErrorMessage: 'tabContext failed: tab not found'},
    );
  }

  // Helper for `testFetchInactiveTabScreenshot` and
  // `testFetchInactiveTabScreenshotWhileMinimized`.
  async fetchInactiveTabScreenshot() {
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
        (f) => !!f.hasFocus && f.hasFocus.tabData.tabId !== tabId);

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
    const context = await this.fetchInactiveTabScreenshot();
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

  async testSwitchConversationToOldConversationNewInstance() {
    assertDefined(this.host.switchConversation);
    await this.host.switchConversation(
        {conversationId: 'A', conversationTitle: 'Title A'});
  }

  async testSwitchConversationToNewConversationNewInstance() {
    assertDefined(this.host.switchConversation);
    await this.host.switchConversation();
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

  async testReloadWebUi() {}

  private async assertCreateTabFails(url: string) {
    assertDefined(this.host.createTab);
    await assertRejects(
        this.host.createTab(url, {openInBackground: false}),
        {withErrorMessage: 'createTab: failed'});
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

  async testSendsViewChangeRequestOnTaskIconOrGlicButtonToggle() {
    assertDefined(this.host.getViewChangeRequests);
    assertDefined(this.host.onViewChanged);
    // Set up observer before the request will be sent.
    const viewChangeRequests =
        observeSequence<ViewChangeRequest>(this.host.getViewChangeRequests());

    await this.advanceToNextStep();
    const actuationChangeRequest = await viewChangeRequests.next();
    assertDefined(actuationChangeRequest.desiredView);
    assertEquals(actuationChangeRequest.desiredView, ClientView.ACTUATION);
    this.host.onViewChanged({currentView: actuationChangeRequest.desiredView});

    await this.advanceToNextStep();
    const conversationChangeRequest = await viewChangeRequests.next();
    assertDefined(conversationChangeRequest.desiredView);
    assertEquals(
        conversationChangeRequest.desiredView, ClientView.CONVERSATION);
  }

  async testRemoveBlankInstanceOnClose() {
    assertDefined(this.host.closePanel);
    await this.host.closePanel();
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

  async testGetModelQualityClientIdFeatureEnabled() {
    assertDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await this.host.getHostCapabilities();
    assertTrue(capabilities.has(HostCapability.GET_MODEL_QUALITY_CLIENT_ID));

    assertDefined(this.host.getModelQualityClientId);
    const clientId = await this.host.getModelQualityClientId();
    assertDefined(clientId);
  }

  async testGetModelQualityClientIdFeatureDisabled() {
    assertDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await this.host.getHostCapabilities();
    assertFalse(capabilities.has(HostCapability.GET_MODEL_QUALITY_CLIENT_ID));

    assertUndefined(this.host.getModelQualityClientId);
  }

  /**
   * A basic test to verify that `getPageMetadata` correctly retrieves metadata
   * for a given tab.
   */
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
    await metadataSequence.completed;
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

    const metadataSequence = observeSequence(metadataObservable1);
    const metadata: PageMetadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    assertEquals(1, metadata.frameMetadata[0]!.metaTags.length);
    // Should be 'author', not 'description'.
    assertEquals('author', metadata.frameMetadata[0]!.metaTags[0]!.name);
  }

  /**
   * Tests that the `ObservableValue` returned by `getPageMetadata` emits new
   * values when the page's metadata changes. This test uses corresponding
   * C++ changes to modify the metadata.
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

    let metadata: PageMetadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    let authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('George', authorTag.content);

    // C++ side will change the meta tag.
    await this.advanceToNextStep();

    metadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    authorTag =
        metadata.frameMetadata[0]!.metaTags.find(tag => tag.name === 'author');
    assertDefined(authorTag);
    assertEquals('Ruth', authorTag.content);
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

    // The C++ side will navigate to a page with no meta tags.
    await this.advanceToNextStep();

    metadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata.length);
    assertEquals(0, metadata.frameMetadata[0]!.metaTags.length);
  }

  /**
   * Checks that the `ObservableValue` stops emitting updates after the
   * associated tab is closed.
   */
  async testGetPageMetadataTabDestroyed() {
    assertDefined(this.host.getPageMetadata);
    assertDefined(this.host.getFocusedTabStateV2);

    const focus =
        await observeSequence(this.host.getFocusedTabStateV2()).next();
    const tabId = checkDefined(focus.hasFocus?.tabData.tabId);

    const metadataObservable = this.host.getPageMetadata(tabId, ['author']);
    assertDefined(metadataObservable);
    const metadataSequence = observeSequence(metadataObservable);

    const metadata: PageMetadata = await metadataSequence.next();
    assertDefined(metadata);
    assertEquals(1, metadata.frameMetadata[0]!.metaTags.length);

    // Close the tab.
    await this.advanceToNextStep();

    // The observable should not emit any more values, and should complete.
    await metadataSequence.completed;
    assertTrue(metadataSequence.isEmpty());
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

  async testAdditionalContext() {
    const additionalContextPromise = new Promise<void>(resolve => {
      this.host.getAdditionalContext!().subscribe(async context => {
        assertEquals(context.name, 'part with everything');
        assertDefined(context.tabId);
        assertTrue(context.tabId!.length > 0);
        assertDefined(context.frameUrl);
        assertTrue(context.frameUrl!.length > 0);
        assertEquals(context.parts.length, 6);

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
        resolve();
      });
    });

    await this.advanceToNextStep();
    await additionalContextPromise;
  }

  async testCaptureRegion() {
    assertDefined(this.host.captureRegion);
    const captureRegionPromise = Promise.withResolvers<void>();
    const observable = this.host.captureRegion();
    assertDefined(observable);
    const subscription = observable.subscribeObserver!({
      next: (result: CaptureRegionResult) => {
        subscription.unsubscribe();
        assertDefined(result);
        assertDefined(result.tabId);
        assertDefined(result.region?.rect);
        assertEquals(10, result.region.rect.x);
        assertEquals(20, result.region.rect.y);
        assertEquals(30, result.region.rect.width);
        assertEquals(40, result.region.rect.height);
        captureRegionPromise.resolve();
      },
    });

    await this.advanceToNextStep();
    await waitFor(captureRegionPromise.promise);
  }

  async testCaptureRegionMultiple() {
    assertDefined(this.host.captureRegion);
    const observable = this.host.captureRegion();
    assertDefined(observable);
    const sequence = observeSequence(observable);

    // Let C++ know captureRegion has been called.
    await this.advanceToNextStep();

    const result1 = await sequence.next();
    assertDefined(result1);
    assertDefined(result1.region?.rect);
    assertEquals(10, result1.region.rect.x);
    assertEquals(20, result1.region.rect.y);
    assertEquals(30, result1.region.rect.width);
    assertEquals(40, result1.region.rect.height);

    // Let C++ know we're ready for the next one.
    await this.advanceToNextStep();

    const result2 = await sequence.next();
    assertDefined(result2);
    assertDefined(result2.region?.rect);
    assertEquals(50, result2.region.rect.x);
    assertEquals(60, result2.region.rect.y);
    assertEquals(70, result2.region.rect.width);
    assertEquals(80, result2.region.rect.height);

    sequence.unsubscribe();
  }

  async testCaptureRegionCancelBrowser() {
    assertDefined(this.host.captureRegion);
    const errorPromise = Promise.withResolvers<void>();
    const observable = this.host.captureRegion();
    assertDefined(observable);
    observable.subscribeObserver!({
      next: () => {
        throw new ApiTestError('Should not have received a result');
      },
      error: (e: any) => {
        assertEquals('captureRegion', e.reasonType);
        assertEquals(CaptureRegionErrorReason.UNKNOWN, e.reason);
        errorPromise.resolve();
      },
    });

    // Let C++ side know to cancel.
    await this.advanceToNextStep();
    await waitFor(errorPromise.promise);
  }

  async testCaptureRegionNoFocus() {
    assertDefined(this.host.captureRegion);
    // In multi-instance mode, detach the panel so it doesn't close with the
    // browser window.
    await this.detachIfInMultiInstance();
    await this.advanceToNextStep();

    const errorPromise = Promise.withResolvers<void>();
    const observable = this.host.captureRegion();
    assertDefined(observable);
    observable.subscribeObserver!({
      next: () => {
        throw new ApiTestError('Should not have received a result');
      },
      error: (e: any) => {
        assertEquals('captureRegion', e.reasonType);
        assertEquals(CaptureRegionErrorReason.NO_FOCUSABLE_TAB, e.reason);
        errorPromise.resolve();
      },
    });
    await waitFor(errorPromise.promise);
  }

  async testCaptureRegionCalledTwice() {
    assertDefined(this.host.captureRegion);
    const completePromise = Promise.withResolvers<void>();
    const resultPromise = Promise.withResolvers<CaptureRegionResult>();

    const obs1 = this.host.captureRegion();
    assertDefined(obs1);
    const sub1 = obs1.subscribeObserver!({
      next: () => {
        throw new ApiTestError('obs1 should not have received a result');
      },
      error: (err) => {
        throw new ApiTestError(
            `obs1 should not have received an error: ${err}`);
      },
      complete: () => {
        completePromise.resolve();
      },
    });

    const obs2 = this.host.captureRegion();
    assertDefined(obs2);
    const sub2 = obs2.subscribeObserver!({
      next: (result: CaptureRegionResult) => {
        resultPromise.resolve(result);
      },
      error: (err) => {
        throw new ApiTestError(
            `obs2 should not have received an error: ${err}`);
      },
    });

    await waitFor(completePromise.promise);
    // Let C++ side know to send a region for obs2.
    await this.advanceToNextStep();
    const result = await waitFor(resultPromise.promise);
    assertDefined(result);
    const rect = checkDefined(result.region?.rect);
    assertEquals(10, rect.x);
    assertEquals(20, rect.y);
    assertEquals(30, rect.width);
    assertEquals(40, rect.height);
    sub1.unsubscribe();
    sub2.unsubscribe();
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

  private async closePanelAndWaitUntilInactive() {
    assertDefined(this.host.closePanel);
    await this.host.closePanel();
    await observeSequence(this.host.panelActive()).waitForValue(false);
  }

  private capabilitiesToString(capabilities: HostCapability[]): string {
    return `[${capabilities.map(this.capabilityToString).join(',')}]`;
  }

  private capabilityToString(capability: HostCapability): string {
    switch (capability) {
      case HostCapability.SCROLL_TO_PDF:
        return 'SCROLL_TO_PDF';
      case HostCapability.RESET_SIZE_AND_LOCATION_ON_OPEN:
        return 'RESET_SIZE_AND_LOCATION_ON_OPEN';
      case HostCapability.GET_MODEL_QUALITY_CLIENT_ID:
        return 'GET_MODEL_QUALITY_CLIENT_ID';
      case HostCapability.MULTI_INSTANCE:
        return 'MULTI_INSTANCE';
      default:
        return 'NEW_ENUM_NOT_IMPLEMENTED';
    }
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
    assertDefined(this.host.getFocusedTabStateV2);
    const focusedTabStateV2Sequence =
        observeSequence<FocusedTabData>(this.host.getFocusedTabStateV2());
    let focusedTabState = await focusedTabStateV2Sequence.next();
    assertDefined(focusedTabState.hasNoFocus);
    const tabStatePromise = focusedTabStateV2Sequence.next();
    assertRejects(waitFor(tabStatePromise, 200));
    // We should only see the second page.
    await this.advanceToNextStep();
    focusedTabState = await tabStatePromise;
    assertDefined(focusedTabState.hasFocus);
    assertEquals(
        new URL(focusedTabState.hasFocus.tabData.url).pathname,
        '/scrollable_page_with_content.html',
        `url=${focusedTabState.hasFocus.tabData.url}`);
  }

  async testNoExtractionWhileHidden() {
    assertDefined(this.host.getContextFromFocusedTab);
    assertDefined(this.host.getContextFromTab);
    assertDefined(this.host.getFocusedTabStateV2);
    assertDefined(this.host.pinTabs);
    await this.host.setTabContextPermissionState(true);

    // While still hidden (preloaded), focused tab extraction should fail.
    await assertRejects(this.host.getContextFromFocusedTab({}), {
      withErrorMessage:
          'GetContextFromFocusedTab not allowed while backgrounded',
    });

    // Glic panel is open, so both focused and arbitrary tab extraction should
    // succeed.
    await this.advanceToNextStep();
    await this.client.waitForFirstOpen();
    let result = await this.host.getContextFromFocusedTab({});
    assertDefined(result);
    assertEquals(
        new URL(result.tabData.url).pathname, '/glic/browser_tests/test.html',
        `Tab data has unexpected url ${result.tabData.url}`);
    const focusedTab = await this.host.getFocusedTabStateV2().getCurrentValue();
    const tabId = checkDefined(focusedTab?.hasFocus?.tabData.tabId);
    assertTrue(await this.host.pinTabs([tabId]));
    result = await this.host.getContextFromTab(tabId, {});
    assertDefined(result);
    assertEquals(
        new URL(result.tabData.url).pathname, '/glic/browser_tests/test.html',
        `Tab data has unexpected url ${result.tabData.url}`);

    // Glic panel is hidden again. Focused and arbitrary tab extraction should
    // fail.
    await this.advanceToNextStep();
    // Panel closure was only requested by native code, but still needs to be
    // waited on.
    await observeSequence(this.host.panelActive()).waitForValue(false);
    await assertRejects(this.host.getContextFromFocusedTab({}), {
      withErrorMessage:
          'GetContextFromFocusedTab not allowed while backgrounded',
    });
    await assertRejects(this.host.getContextFromTab(tabId, {}), {
      withErrorMessage: 'GetContextFromTab not allowed while backgrounded',
    });
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
  DaisyChainApiTests,
  NotifyPanelWillOpenTest,
  InitiallyNotResizableTest,
  ApiTestWithoutOpen,
  ApiTestFailsToInitialize,
];

testMain(TEST_FIXTURES);
