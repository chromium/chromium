// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutocompleteMatch, PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/omnibox/omnibox.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export function createAutocompleteMatch(): AutocompleteMatch {
  return {
    a11yLabel: {data: []},
    actions: [],
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
    contents: {data: []},
    contentsClass: [{offset: 0, style: 0}],
    description: {data: []},
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: {url: ''},
    inlineAutocompletion: {data: []},
    fillIntoEdit: {data: []},
    iconUrl: '',
    imageDominantColor: '',
    imageUrl: '',
    removeButtonA11yLabel: {data: []},
    type: '',
    isRichSuggestion: false,
  };
}

export class TestRealboxBrowserProxy extends TestBrowserProxy {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    super([]);
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
  }
}
