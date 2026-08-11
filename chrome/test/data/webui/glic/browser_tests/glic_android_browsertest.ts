// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiTestFixtureBase, assertDefined, assertTrue, testMain} from './browser_test_base.js';

class GlicAndroidBrowserTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testPageContextFetching() {
    const result = await this.host.getContextFromFocusedTab?.({
      viewportScreenshot: false,
    });

    assertDefined(result);
    assertTrue(
        result.tabData.url.endsWith('/page.html'),
        `Tab data has unexpected url ${result.tabData.url}`);
  }

  async testDeviceRotationMojoResiliency() {
    assertDefined(this.host);
  }
}

testMain([
  GlicAndroidBrowserTests,
]);
