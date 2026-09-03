// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/host/glic_chrome_tools_browsertest.cc

import {ExecuteToolErrorReason, HostCapability} from '/glic/glic_api/glic_api.js';
import type {ChromeTool} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertTrue, assertUndefined, checkDefined, testMain} from './browser_test_base.js';

class GlicChromeToolsDisabledBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testGetChromeToolsDisabled() {
    const getHostCapabilities = checkDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await getHostCapabilities.call(this.host);
    assertFalse(capabilities.has(HostCapability.CHROME_TOOLS));

    assertUndefined(this.host.tools);
  }
}

class GlicChromeToolsBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testGetChromeTools() {
    const getHostCapabilities = checkDefined(this.host.getHostCapabilities);
    const capabilities: Set<HostCapability> =
        await getHostCapabilities.call(this.host);
    assertTrue(capabilities.has(HostCapability.CHROME_TOOLS));

    const toolsAccessor = checkDefined(this.host.tools);
    const toolsHost = checkDefined(toolsAccessor.call(this.host));
    const getChromeTools = checkDefined(toolsHost.getChromeTools);
    const tools = await getChromeTools.call(toolsHost);
    assertDefined(tools);
    assertTrue(tools.length > 0);
    const toolNames = tools.map((t: ChromeTool) => t.name);
    assertTrue(toolNames.includes('open_url'));
  }

  async testExecuteToolNotFound() {
    const toolsAccessor = checkDefined(this.host.tools);
    const toolsHost = checkDefined(toolsAccessor.call(this.host));
    const executeTool = checkDefined(toolsHost.executeTool);
    const result = await executeTool.call(toolsHost, 'nonexistent_tool', '{}');
    assertEquals(result.errorReason, ExecuteToolErrorReason.TOOL_NOT_FOUND);
  }

  async testExecuteToolInvalidArguments() {
    const toolsAccessor = checkDefined(this.host.tools);
    const toolsHost = checkDefined(toolsAccessor.call(this.host));
    const executeTool = checkDefined(toolsHost.executeTool);
    const result =
        await executeTool.call(toolsHost, 'open_url', 'invalid json {{{');
    assertEquals(result.errorReason, ExecuteToolErrorReason.INVALID_ARGUMENTS);
  }

  async testExecuteToolSuccess() {
    const toolsAccessor = checkDefined(this.host.tools);
    const toolsHost = checkDefined(toolsAccessor.call(this.host));
    const executeTool = checkDefined(toolsHost.executeTool);
    const result = await executeTool.call(
        toolsHost, 'open_url',
        JSON.stringify({url: 'https://example.com', new_tab: true}));
    assertUndefined(result.errorReason);
    assertEquals(result.result, '{}');
  }
}

const TEST_FIXTURES = [
  GlicChromeToolsDisabledBrowserTest,
  GlicChromeToolsBrowserTest,
];

testMain(TEST_FIXTURES);
