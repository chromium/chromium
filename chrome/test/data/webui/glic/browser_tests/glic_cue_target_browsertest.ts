// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FreOverride, type InvokeOptions, type OpenPanelInfo, type PanelOpeningData} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertEquals, assertTrue, runUntil, testMain, WebClient} from './browser_test_base.js';

class InvokeClient extends WebClient {
  receivedOptions: InvokeOptions|null = null;
  receivedPanelOpeningData: PanelOpeningData|null = null;

  override async notifyPanelWillOpen(panelOpeningData: PanelOpeningData):
      Promise<OpenPanelInfo> {
    this.receivedPanelOpeningData = panelOpeningData;
    return super.notifyPanelWillOpen(panelOpeningData);
  }

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
    await runUntil(
        () => client.receivedOptions !== null &&
            client.receivedPanelOpeningData !== null);
    assertTrue(client.receivedOptions!.autoSubmit === true);
    assertEquals(
        'test prompt auto submit', client.receivedOptions!.prompts?.[0]);
    assertEquals(
        FreOverride.UNSPECIFIED, client.receivedPanelOpeningData!.freOverride);
  }

  async testOnClickAutoSubmitDisabled() {
    const client = this.client as InvokeClient;
    await runUntil(
        () => client.receivedOptions !== null &&
            client.receivedPanelOpeningData !== null);
    assertTrue(client.receivedOptions!.autoSubmit === false);
    assertEquals(
        'test prompt no auto submit', client.receivedOptions!.prompts?.[0]);
    assertEquals(
        FreOverride.UNSPECIFIED, client.receivedPanelOpeningData!.freOverride);
  }

  async testOnEditPrompt() {
    const client = this.client as InvokeClient;
    await runUntil(
        () => client.receivedOptions !== null &&
            client.receivedPanelOpeningData !== null);
    assertTrue(client.receivedOptions!.autoSubmit === false);
    assertEquals(
        'test prompt edit prompt', client.receivedOptions!.prompts?.[0]);
    assertEquals(
        FreOverride.UNSPECIFIED, client.receivedPanelOpeningData!.freOverride);
  }

  async testOnClickMessageFirstFreEnabled() {
    const client = this.client as InvokeClient;
    await runUntil(
        () => client.receivedOptions !== null &&
            client.receivedPanelOpeningData !== null);
    assertTrue(client.receivedOptions!.autoSubmit === true);
    assertEquals(
        'test prompt message first fre', client.receivedOptions!.prompts?.[0]);
    assertEquals(
        FreOverride.TRUST_FIRST_INLINE,
        client.receivedPanelOpeningData!.freOverride);
  }

  async testOnClickMessageFirstFreEnabledHasConsented() {
    const client = this.client as InvokeClient;
    await runUntil(
        () => client.receivedOptions !== null &&
            client.receivedPanelOpeningData !== null);
    assertTrue(client.receivedOptions!.autoSubmit === true);
    assertEquals(
        'test prompt message first fre consented',
        client.receivedOptions!.prompts?.[0]);
    assertEquals(
        FreOverride.UNSPECIFIED, client.receivedPanelOpeningData!.freOverride);
  }

  async testOnClickMessageFirstFreEnabledAutoSubmitDisabled() {
    const client = this.client as InvokeClient;
    await runUntil(
        () => client.receivedOptions !== null &&
            client.receivedPanelOpeningData !== null);
    assertTrue(client.receivedOptions!.autoSubmit === false);
    assertEquals(
        'test prompt message first fre no auto submit',
        client.receivedOptions!.prompts?.[0]);
    assertEquals(
        FreOverride.UNSPECIFIED, client.receivedPanelOpeningData!.freOverride);
  }
}

testMain([
  GlicCueTargetApiTest,
]);
