// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InvokeOptions} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertEquals, assertTrue, runUntil, testMain, WebClient} from './browser_test_base.js';

class InvokeClient extends WebClient {
  receivedOptions: InvokeOptions|null = null;

  override async invoke(options: InvokeOptions): Promise<void> {
    this.receivedOptions = options;
  }
}

class GlicCueTargetApiTest extends ApiTestFixtureBase {
  override createWebClient() {
    return new InvokeClient();
  }

  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testOnClickAutoSubmitEnabled() {
    const client = this.client as InvokeClient;
    await runUntil(() => client.receivedOptions !== null);
    assertTrue(client.receivedOptions!.autoSubmit === true);
    assertEquals(
        'test prompt auto submit', client.receivedOptions!.prompts?.[0]);
  }

  async testOnClickAutoSubmitDisabled() {
    const client = this.client as InvokeClient;
    await runUntil(() => client.receivedOptions !== null);
    assertTrue(client.receivedOptions!.autoSubmit === false);
    assertEquals(
        'test prompt no auto submit', client.receivedOptions!.prompts?.[0]);
  }

  async testOnEditPrompt() {
    const client = this.client as InvokeClient;
    await runUntil(() => client.receivedOptions !== null);
    assertTrue(client.receivedOptions!.autoSubmit === false);
    assertEquals(
        'test prompt edit prompt', client.receivedOptions!.prompts?.[0]);
  }
}

testMain([
  GlicCueTargetApiTest,
]);
