// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeToolBlockingBehavior} from '/glic/glic_api/glic_api.js';
import type {ChromeTool, ChromeToolExecutionResult} from '/glic/glic_api/glic_api.js';

import {getBrowser, logMessage} from '../client.js';
import {$} from '../page_element_types.js';

const currentToolsMap = new Map<string, ChromeTool>();

$.getChromeToolsBtn.addEventListener('click', async () => {
  const toolsHost = getBrowser()?.tools?.();
  if (!toolsHost || !toolsHost.getChromeTools) {
    $.toolsStatus.innerText = 'getChromeTools() is not available on this host.';
    logMessage('getChromeTools() is not supported.');
    return;
  }

  $.toolsStatus.innerText = 'Querying Chrome Tools...';
  try {
    const tools = await toolsHost.getChromeTools();
    currentToolsMap.clear();
    while ($.toolSelect.options.length > 0) {
      $.toolSelect.remove(0);
    }

    if (!tools || tools.length === 0) {
      $.toolsStatus.innerText = 'No tools returned from host.';
      const option = document.createElement('option');
      option.value = '';
      option.text = '-- No tools loaded --';
      $.toolSelect.add(option);
      return;
    }

    tools.forEach((tool: ChromeTool) => {
      currentToolsMap.set(tool.name, tool);
      const option = document.createElement('option');
      option.value = tool.name;
      const behaviorLabel = ChromeToolBlockingBehavior[tool.blockingBehavior] ??
          String(tool.blockingBehavior);
      option.text = `${tool.name} (${behaviorLabel})`;
      $.toolSelect.add(option);
    });

    $.toolsStatus.innerText = `Loaded ${tools.length} tool(s):\n` +
        tools.map(t => `- ${t.name}: ${t.description}`).join('\n');
    logMessage(`getChromeTools returned ${tools.length} tools.`);

    // Trigger selection change for initial tool.
    onToolSelected();
  } catch (e) {
    $.toolsStatus.innerText = `Error querying tools: ${e}`;
    logMessage(`getChromeTools failed: ${e}`);
  }
});

function onToolSelected() {
  const selectedName = $.toolSelect.value;
  const tool = currentToolsMap.get(selectedName);
  if (!tool) {
    return;
  }

  // Pre-populate sample JSON arguments based on the tool.
  let sampleArgs = '{}';
  switch (selectedName) {
    case 'open_url':
      sampleArgs =
          JSON.stringify({url: 'https://chromium.org', new_tab: true}, null, 2);
      break;
    case 'perform_search':
      sampleArgs = JSON.stringify({query: 'example', new_tab: true}, null, 2);
      break;
    case 'switch_tab':
    case 'find_and_highlight':
    case 'open_page':
      sampleArgs = JSON.stringify({query: 'example'}, null, 2);
      break;
    case 'follow_link':
      sampleArgs = JSON.stringify({id: '1'}, null, 2);
      break;
    case 'scroll':
      sampleArgs = JSON.stringify({granularity: 'page', magnitude: 1}, null, 2);
      break;
    case 'seek_to_timestamp':
      sampleArgs = JSON.stringify({timecode: '01:30'}, null, 2);
      break;
    case 'translate_page':
      sampleArgs = JSON.stringify({target_language: 'es'}, null, 2);
      break;
    case 'set_text':
      sampleArgs = JSON.stringify({dom_node_id: 101, text: 'example'}, null, 2);
      break;
    case 'click_element':
      sampleArgs = JSON.stringify({dom_node_id: 101}, null, 2);
      break;
    case 'set_fullscreen':
      sampleArgs = JSON.stringify({fullscreen: true}, null, 2);
      break;
    case 'select_option':
      sampleArgs =
          JSON.stringify({dom_node_id: 101, value: 'example'}, null, 2);
      break;
    case 'open_gemini_panel':
    case 'invoke_glic':
      sampleArgs = JSON.stringify({prompt: 'Hello from tool test!'}, null, 2);
      break;
    case 'close_gemini_panel':
      sampleArgs = '{}';
      break;
    default:
      if (tool.jsonSchemaParameters && tool.jsonSchemaParameters !== '{}') {
        sampleArgs = tool.jsonSchemaParameters;
      } else {
        sampleArgs = '{}';
      }
      break;
  }
  $.toolArgumentsInput.value = sampleArgs;
}

$.toolSelect.addEventListener('change', onToolSelected);

$.executeToolBtn.addEventListener('click', async () => {
  const toolsHost = getBrowser()?.tools?.();
  if (!toolsHost || !toolsHost.executeTool) {
    $.executeToolResult.value = 'executeTool() is not available on this host.';
    logMessage('executeTool() is not supported.');
    return;
  }

  const toolName = $.toolSelect.value;
  if (!toolName) {
    $.executeToolResult.value = 'Please query and select a tool first.';
    return;
  }

  const jsonArguments = $.toolArgumentsInput.value || '{}';
  $.executeToolResult.value = `Executing ${toolName}...`;
  logMessage(`Executing tool ${toolName} with args: ${jsonArguments}`);

  try {
    const result: ChromeToolExecutionResult =
        await toolsHost.executeTool(toolName, jsonArguments);
    $.executeToolResult.value = JSON.stringify(result, null, 2);
    logMessage(`executeTool(${toolName}) completed with errorReason=${
        result.errorReason}, scheduling=${result.scheduling}`);
  } catch (e) {
    $.executeToolResult.value = `Exception calling executeTool: ${e}`;
    logMessage(`executeTool(${toolName}) exception: ${e}`);
  }
});
