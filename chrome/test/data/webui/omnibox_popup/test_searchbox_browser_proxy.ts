// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestSearchboxBrowserProxy {
  callbackRouter: SearchboxPageCallbackRouter;
  handler: TestMock<SearchboxPageHandlerRemote>&SearchboxPageHandlerRemote;
  page: SearchboxPageRemote;

  constructor() {
    this.callbackRouter = new SearchboxPageCallbackRouter();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = TestMock.fromClass(SearchboxPageHandlerRemote);
    this.handler.setPromiseResolveFor<'getRecentTabs'>(
        'getRecentTabs', {tabs: []});
    this.handler.setPromiseResolveFor<'getInputState'>('getInputState', {
      state: {
        allowedModels: [],
        allowedTools: [],
        allowedInputTypes: [],
        activeModel: 0,  // kUnspecified
        activeTool: 0,   // kUnspecified
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
      },
    });
  }

  initVisibilityPrefs() {
    this.page.updateAimEligibility(true);
    this.page.onShowAiModePrefChanged(true);
    this.page.updateContentSharingPolicy(true);
  }
}
