// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CancelActionsResult, ClientCapabilities, ExperimentalTriggeringUpdateType, FileUploadPolicyState, FormFactor, HostCapability, PanelStateKind, Platform, SbThreatType, ScreenshotEncryptionScheme, ScrollToErrorReason, SkillSource, WebClientMode} from '/glic/glic_api/glic_api.js';
import type {AdditionalContext, CounterAbuseVerdict, ExperimentalTriggeringUpdate, FocusedTabData, GetPinCandidatesOptions, GlicBrowserHost, GlicWebClient, InvokeOptions, Observable, Observable2, OpenPanelInfo, PageMetadata, PanelOpeningData, PanelState, ScrollToError, TabData, UserProfileInfo, ZeroStateSuggestionsV2} from '/glic/glic_api/glic_api.js';
import {Subject} from '/glic/observable.js';

import {ApiTestError, ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertRejects, assertTrue, assertUndefined, checkDefined, mapObservable, observeSequence, runUntil, sleep, testMain, waitFor, WebClient} from './browser_test_base.js';


class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testDoNothing() {}

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

    // Open another tab so prodUrl is no longer the active tab.
    const blankUrl = location.href + '#blank';
    const createdBlank = await this.host.createTab(blankUrl, {});
    assertEquals(createdBlank.url, blankUrl);

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
    this.host.setAudioDucking(true);
    const link = document.createElement('a');
    link.setAttribute('href', 'https://www.chromium.org');
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();

    await this.advanceToNextStep();
    this.host.setAudioDucking(false);
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
  override async notifyPanelWillOpen(
      panelOpeningData: PanelOpeningData&PanelState): Promise<OpenPanelInfo> {
    this.calls.push('notifyPanelWillOpen');
    return super.notifyPanelWillOpen!(panelOpeningData);
  }

  override async invoke(_options: InvokeOptions): Promise<void> {
    this.calls.push('invoke');
  }
}

class InvokeTest extends ApiTests {
  override createWebClient() {
    return new InvokeClient();
  }

  async testInvokeWaitsForNotifyPanelWillOpen() {
    const client: InvokeClient = this.client as InvokeClient;
    await runUntil(() => {
      return client.calls.length === 2;
    });

    assertEquals('notifyPanelWillOpen,invoke', client.calls.join(','));
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
    panelOpenState.waitForValue(true);
    await this.host.closePanel!();
    panelOpenState.waitForValue(false);
    await this.advanceToNextStep();
    openSignal.resolve();
    await panelOpenState.waitForValue(true);
  }
}

class SkillsApiTests extends ApiTests {
  async testGetSkillSuccess() {
    assertDefined(this.host.getSkillPreviews);
    assertDefined(this.host.getSkill);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    const targetSkill = skills.find(s => s.name === 'test_skill_1');
    assertDefined(targetSkill);
    const actualSkill = await this.host.getSkill(targetSkill.id);
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
    const actualSkill1 = await this.host.getSkill(skill1.id);
    assertDefined(actualSkill1);
    assertEquals(actualSkill1.sourceSkillId, 'source_id_1');
    const skill2 = skills.find(s => s.name === 'test_skill_2');
    assertDefined(skill2);
    assertEquals('test_icon_2', skill2.icon);
    const actualSkill2 = await this.host.getSkill(skill2.id);
    assertDefined(actualSkill2);
    assertEquals(actualSkill2.sourceSkillId, 'source_id_2');
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

const TEST_FIXTURES: Array<typeof ApiTestFixtureBase> = [
  ApiTests,
  AdditionalContextQueuedTest,
  FaviconTest,
  FaviconOmittedTest,
  InvokeTest,
  ApiTestFailsToInitialize,
  TriggeringUpdatesTest,
  ScreenshotTests,
];


if (!navigator.userAgent.includes('Android')) {
  TEST_FIXTURES.push(SkillsApiTests, InitiallyNotResizableTest);
}

testMain(TEST_FIXTURES);
