// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter as OmniboxEverywherePageCallbackRouter, PageHandlerRemote as OmniboxEverywherePageHandlerRemote} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.mojom-webui.js';
import type {PageRemote as OmniboxEverywherePageRemote} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export function createDefaultInputState(): InputState {
  return {
    allowedModels: [],
    allowedTools: [],
    allowedInputTypes: [],
    activeModel: 0,  // kUnspecified
    activeTool: 0,   // kUnspecified
    disabledModels: [],
    disabledTools: [],
    disabledInputTypes: [],
    toolConfigs: [],
    modelConfigs: [],
    inputTypeConfigs: [],
    toolsSectionConfig: null,
    modelSectionConfig: null,
    hintText: '',
    maxInputsByType: {},
    maxTotalInputs: 0,
    isCanvasQuerySubmitted: false,
  };
}

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
      state: createDefaultInputState(),
    });
    this.handler.setPromiseResolveFor<'getSmartTabSharingActive'>(
        'getSmartTabSharingActive', {active: false});
    this.handler.setPromiseResolveFor<'getPageClassification'>(
        'getPageClassification', {metricSource: 'OMNIBOX_EVERYWHERE'});
    this.handler.setPromiseResolveFor<'startScreenshare'>(
        'startScreenshare', {token: null});
    this.handler.setPromiseResolveFor<'captureRegionScreenshot'>(
        'captureRegionScreenshot', {token: null});
  }

  initVisibilityPrefs() {
    this.page.updateAimPopupEligibility(true);
    this.page.updateContentSharingPolicy(true);
  }
}

export class TestOmniboxEverywhereBrowserProxy {
  callbackRouter: OmniboxEverywherePageCallbackRouter;
  handler: TestMock<OmniboxEverywherePageHandlerRemote>&
      OmniboxEverywherePageHandlerRemote;
  page: OmniboxEverywherePageRemote;

  constructor() {
    this.callbackRouter = new OmniboxEverywherePageCallbackRouter();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = TestMock.fromClass(OmniboxEverywherePageHandlerRemote);
  }
}
