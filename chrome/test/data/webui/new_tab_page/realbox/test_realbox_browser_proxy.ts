// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationPredictor, PageCallbackRouter, PageHandlerInterface, PageRemote} from 'chrome://resources/cr_components/omnibox/omnibox.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {Size} from 'chrome://resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
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
      'onNavigationLikely',
      'openAutocompleteMatch',
      'queryAutocomplete',
      'stopAutocomplete',
      'toggleSuggestionGroupIdVisibility',
      'onFocusChanged',
      'popupElementSizeChanged',
    ]);
  }

  setPage(page: PageRemote) {
    this.methodCalled('setPage', page);
  }

  onFocusChanged(focused: boolean) {
    this.methodCalled('onFocusChanged', {focused});
  }

  popupElementSizeChanged(size: Size) {
    this.methodCalled('popupElementSizeChanged', {size});
  }

  deleteAutocompleteMatch(line: number, url: Url) {
    this.methodCalled('deleteAutocompleteMatch', {line, url});
  }

  executeAction(
      line: number, actionIndex: number, url: Url,
      matchSelectionTimestamp: TimeTicks, mouseButton: number, altKey: boolean,
      ctrlKey: boolean, metaKey: boolean, shiftKey: boolean) {
    this.methodCalled('executeAction', {
      line,
      actionIndex,
      url,
      matchSelectionTimestamp,
      mouseButton,
      altKey,
      ctrlKey,
      metaKey,
      shiftKey,
    });
  }

  openAutocompleteMatch(
      line: number, url: Url, areMatchesShowing: boolean, mouseButton: number,
      altKey: boolean, ctrlKey: boolean, metaKey: boolean, shiftKey: boolean) {
    this.methodCalled('openAutocompleteMatch', {
      line,
      url,
      areMatchesShowing,
      mouseButton,
      altKey,
      ctrlKey,
      metaKey,
      shiftKey,
    });
  }

  onNavigationLikely(
      line: number, url: Url, navigationPredictor: NavigationPredictor) {
    this.methodCalled('onNavigationLikely', {line, url, navigationPredictor});
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
