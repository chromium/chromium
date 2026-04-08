// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiTestFixtureBase, runUntil, testMain} from './browser_test_base.js';

class GlicFocusInteractiveTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testFocusOnSidePanelOpen() {
    await runUntil(() => document.activeElement?.id === 'inputBox');
    await runUntil(() => document.hasFocus());
  }
}

const TEST_FIXTURES = [
  GlicFocusInteractiveTest,
];

testMain(TEST_FIXTURES);
