// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationPredictor, PageCallbackRouter, PageHandlerInterface, PageRemote} from 'chrome://new-tab-page/omnibox.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {TimeDelta, TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';


/**
 * Helps track realbox browser call arguments. A mocked page handler remote
 * resolves the browser call promises with the arguments as an array making the
 * tests prone to change if the arguments change. This class extends the page
 * handler remote, resolving the browser call promises with named arguments.
 */
class FakePageHandler extends TestBrowserProxy implements PageHandlerInterface {
  constructor() {
    super([
      'deleteAutocompleteMatch',
      'executeAction',
      'logCharTypedToRepaintLatency',
      'onNavigationLikely',
      'openAutocompleteMatch',
      'queryAutocomplete',
      'stopAutocomplete',
      'toggleSuggestionGroupIdVisibility',
    ]);
  }

  setPage(page: PageRemote) {
    this.methodCalled('setPage', page);
  }

  deleteAutocompleteMatch(line: number) {
    this.methodCalled('deleteAutocompleteMatch', {line});
  }

  executeAction(
      line: number, matchSelectionTimestamp: TimeTicks, mouseButton: number,
      altKey: boolean, ctrlKey: boolean, metaKey: boolean, shiftKey: boolean) {
    this.methodCalled('executeAction', {
      line,
      matchSelectionTimestamp,
      mouseButton,
      altKey,
      ctrlKey,
      metaKey,
      shiftKey,
    });
  }

  logCharTypedToRepaintLatency(timeDelta: TimeDelta) {
    this.methodCalled('logCharTypedToRepaintLatency', {timeDelta});
  }

  openAutocompleteMatch(
      line: number, url: Url, areMatchesShowing: boolean,
      timeElapsedSinceLastFocus: TimeDelta, mouseButton: number,
      altKey: boolean, ctrlKey: boolean, metaKey: boolean, shiftKey: boolean) {
    this.methodCalled('openAutocompleteMatch', {
      line,
      url,
      areMatchesShowing,
      timeElapsedSinceLastFocus,
      mouseButton,
      altKey,
      ctrlKey,
      metaKey,
      shiftKey,
    });
  }

  onNavigationLikely(line: number, navigationPredictor: NavigationPredictor) {
    this.methodCalled('onNavigationLikely', {line, navigationPredictor});
  }

  queryAutocomplete(input: String16, preventInlineAutocomplete: boolean) {
    this.methodCalled('queryAutocomplete', {input, preventInlineAutocomplete});
  }

  stopAutocomplete(clearResult: boolean) {
    this.methodCalled('stopAutocomplete', {clearResult});
  }

  toggleSuggestionGroupIdVisibility(suggestionGroupId: number) {
    this.methodCalled('toggleSuggestionGroupIdVisibility', {suggestionGroupId});
  }
}

export class TestRealboxBrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: FakePageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.handler = new FakePageHandler();
  }
}
