// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiTestFixtureBase, testMain} from './browser_test_base.js';

class ApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testDoNothing() {}
}

const TEST_FIXTURES = [
  ApiTests,
];

testMain(TEST_FIXTURES);
