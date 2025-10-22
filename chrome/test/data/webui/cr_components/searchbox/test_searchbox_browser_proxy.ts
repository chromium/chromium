// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {PageHandlerInterface, PageRemote, PlaceholderConfig, SelectedFileInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {BigBuffer} from 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-webui.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';


/**
 * Helps track realbox browser call arguments. A mocked page handler remote
 * resolves the browser call promises with the arguments as an array making the
 * tests prone to change if the arguments change. This class extends the page
 * handler remote, resolving the browser call promises with named arguments.
 */
class FakePageHandler extends TestBrowserProxy implements PageHandlerInterface {
  private results_: Map<string, any> = new Map();

  constructor() {
    super([
      'deleteAutocompleteMatch',
      'activateKeyword',
      'showContextMenu',
      'executeAction',
      'onNavigationLikely',
      'onThumbnailRemoved',
      'openAutocompleteMatch',
      'queryAutocomplete',
      'stopAutocomplete',
      'toggleSuggestionGroupIdVisibility',
      'onFocusChanged',
      'getPlaceholderConfig',
      'getRecentTabs',
      'getTabPreview',
      'notifySessionStarted',
      'notifySessionAbandoned',
      'addFileContext',
      'addTabContext',
      'deleteContext',
      'clearFiles',
      'submitQuery',
    ]);
  }

  setResultFor(methodName: string, result: any) {
    this.results_.set(methodName, result);
  }

  setPage(page: PageRemote) {
    this.methodCalled('setPage', page);
  }

  onFocusChanged(focused: boolean) {
    this.methodCalled('onFocusChanged', {focused});
  }

  deleteAutocompleteMatch(line: number, url: Url) {
    this.methodCalled('deleteAutocompleteMatch', {line, url});
  }

  activateKeyword(
      line: number, url: Url, matchSelectionTimestamp: TimeTicks,
      isMouseEvent: boolean) {
    this.methodCalled('activateKeyword', {
      line,
      url,
      matchSelectionTimestamp,
      isMouseEvent,
    });
  }

  showContextMenu(point: {x: number, y: number}) {
    this.methodCalled('showContextMenu', {point});
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

  onThumbnailRemoved() {
    this.methodCalled('onThumbnailRemoved', {});
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

  getPlaceholderConfig(): Promise<{config: PlaceholderConfig}> {
    this.methodCalled('getPlaceholderConfig');
    return Promise.resolve({
      config: {
        texts: [],
        changeTextAnimationInterval: {microseconds: BigInt(4000) * 1000n},
        fadeTextAnimationDuration: {microseconds: BigInt(250) * 1000n},
      },
    });
  }

  getRecentTabs() {
    this.methodCalled('getRecentTabs');
    if (this.results_.has('getRecentTabs')) {
      return this.results_.get('getRecentTabs');
    }
    return Promise.resolve({tabs: []});
  }

  getTabPreview(tabId: number) {
    this.methodCalled('getTabPreview', {tabId});
    return Promise.resolve({previewDataUrl: ''});
  }

  notifySessionStarted() {
    this.methodCalled('notifySessionStarted');
  }

  notifySessionAbandoned() {
    this.methodCalled('notifySessionAbandoned');
  }

  addFileContext(fileInfo: SelectedFileInfo, fileBytes: BigBuffer) {
    this.methodCalled('addFileContext', {fileInfo, fileBytes});
    return Promise.resolve({token: ''});
  }

  addTabContext(tabId: number) {
    this.methodCalled('addTabContext', {tabId});
    return Promise.resolve({token: ''});
  }

  deleteContext(fileToken: UnguessableToken) {
    this.methodCalled('deleteContext', {fileToken});
  }

  clearFiles() {
    this.methodCalled('clearFiles');
  }

  submitQuery(
      queryText: string, mouseButton: number, altKey: boolean, ctrlKey: boolean,
      metaKey: boolean, shiftKey: boolean) {
    this.methodCalled(
        'submitQuery',
        {queryText, mouseButton, altKey, ctrlKey, metaKey, shiftKey});
  }
}

export class TestSearchboxBrowserProxy {
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
