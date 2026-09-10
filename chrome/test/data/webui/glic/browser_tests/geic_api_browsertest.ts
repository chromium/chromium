// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/gemini_enterprise/geic_api_browsertest.cc

import type {GeicBrowserHost} from '/glic/glic_api/glic_api.js';
import {CloseSignInTabResult, OpenSignInTabResult} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, testMain} from './browser_test_base.js';

class GlicGeicApiBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  get geic(): GeicBrowserHost {
    assertDefined(this.host.getGeicClient);
    const geic = this.host.getGeicClient();
    assertDefined(geic);
    return geic;
  }

  async testGeicSignInTab() {
    // Closing before opening any tab returns NO_SIGN_IN_TAB.
    const initialResult = await this.geic.closeSignInTab();
    assertEquals(initialResult, CloseSignInTabResult.NO_SIGN_IN_TAB);

    // Calling openSignInTab without a URL returns ERROR_NO_URL.
    const noUrlResult = await this.geic.openSignInTab();
    assertEquals(noUrlResult, OpenSignInTabResult.ERROR_NO_URL);

    // Calling openSignInTab with a disallowed URL returns ERROR_DISALLOWED_URL.
    const disallowedResult = await this.geic.openSignInTab(
        {signinUrl: 'https://user:pass@accounts.google.com/signin'});
    assertEquals(disallowedResult, OpenSignInTabResult.ERROR_DISALLOWED_URL);

    // Call openSignInTab with allowed Google signin URL in options.
    const openResult = await this.geic.openSignInTab(
        {signinUrl: 'https://accounts.google.com/signin'});
    assertEquals(openResult, OpenSignInTabResult.SUCCESS);

    // Close the sign-in tab.
    const result = await this.geic.closeSignInTab();
    assertEquals(result, CloseSignInTabResult.SUCCESS);

    // Calling closeSignInTab again returns NO_SIGN_IN_TAB.
    const secondResult = await this.geic.closeSignInTab();
    assertEquals(secondResult, CloseSignInTabResult.NO_SIGN_IN_TAB);
  }
}

testMain([
  GlicGeicApiBrowserTest,
]);
