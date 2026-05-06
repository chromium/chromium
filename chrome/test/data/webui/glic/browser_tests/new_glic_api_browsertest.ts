// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CancelActionsResult, ClientCapabilities, ExperimentalTriggeringUpdateType, SkillSource} from '/glic/glic_api/glic_api.js';
import type {AdditionalContext, ExperimentalTriggeringUpdate, GlicBrowserHost, GlicWebClient, InvokeOptions, Observable, Observable2, OpenPanelInfo, PageMetadata, PanelOpeningData, PanelState, TabData} from '/glic/glic_api/glic_api.js';
import {Subject} from '/glic/observable.js';

import {ApiTestError, ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertRejects, assertTrue, assertUndefined, checkDefined, mapObservable, observeSequence, runUntil, sleep, testMain, waitFor, WebClient} from './browser_test_base.js';


class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testDoNothing() {}

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

  async testInvocationSource() {
    const expectedSource = this.testParams as number;
    await observeSequence(this.client.panelOpenData)
        .waitFor((data) => data && data.invocationSource === expectedSource);
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

  async invoke?(_options: InvokeOptions): Promise<void> {
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

  async testShowManageSkillsUiNoWindow() {
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi();
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

const TEST_FIXTURES = [
  ApiTests,
  AdditionalContextQueuedTest,
  FaviconTest,
  FaviconOmittedTest,
  InvokeTest,
  ApiTestFailsToInitialize,
  TriggeringUpdatesTest,
];


if (!navigator.userAgent.includes('Android')) {
  TEST_FIXTURES.push(SkillsApiTests);
}

testMain(TEST_FIXTURES);
