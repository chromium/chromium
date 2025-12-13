// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiTestFixtureBase, assertTrue, testMain} from './browser_test_base.js';

// Test cases here correspond to test cases in glic_api_interactive_uitest.cc.
// Since these tests run in the webview, this test can't use normal deps like
// mocha or chai assert.
class GlicApiUiTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testOpenGlic() {
    assertTrue(true);
  }
}

// All test fixtures. We look up tests by name, and the fixture name is ignored.
// Therefore all tests must have unique names.
const TEST_FIXTURES = [
  GlicApiUiTest,
];

testMain(TEST_FIXTURES);
