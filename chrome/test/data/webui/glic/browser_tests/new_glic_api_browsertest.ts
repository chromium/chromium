// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClientCapabilities} from '/glic/glic_api/glic_api.js';
import type {GlicWebClient, InvokeOptions, Observable, OpenPanelInfo, PanelOpeningData, PanelState, TabData} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertUndefined, mapObservable, observeSequence, runUntil, testMain, WebClient} from './browser_test_base.js';

class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testDoNothing() {}
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
    await this.advanceToNextStep();

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
    await this.advanceToNextStep();

    // We should see the generic globe icon. Just assert there is a change.
    await faviconColors.waitFor((colors) => colors !== '#00ff');
  }
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

class SkillsApiTests extends ApiTests {
  async testShowBrowseSkillsUi() {
    assertDefined(this.host.showBrowseSkillsUi);
    this.host.showBrowseSkillsUi();
  }
}

const TEST_FIXTURES = [
  ApiTests,
  FaviconTest,
  FaviconOmittedTest,
  InvokeTest,
];

if (!navigator.userAgent.includes('Android')) {
  TEST_FIXTURES.push(SkillsApiTests);
}

testMain(TEST_FIXTURES);
