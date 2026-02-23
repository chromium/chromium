// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './composebox_test_utils.js';

suite('ComposeboxInputPlaceholder', () => {
  let composebox: ComposeboxElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);

    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,
        activeTool: 0,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    }));

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await microtasksFinished();
  });

  test('InputPlaceholderOverride', async () => {
    const input = composebox.$.input;
    assertTrue(!!input);
    const initialPlaceholder = input.placeholder;
    assertTrue(initialPlaceholder.length > 0);
    assertEquals('', composebox.inputPlaceholderOverride);

    const overrideText = 'Override Text';
    composebox.inputPlaceholderOverride = overrideText;
    await composebox.updateComplete;

    assertEquals(overrideText, input.placeholder);

    composebox.inputPlaceholderOverride = '';
    await composebox.updateComplete;

    assertEquals(initialPlaceholder, input.placeholder);
  });
});
