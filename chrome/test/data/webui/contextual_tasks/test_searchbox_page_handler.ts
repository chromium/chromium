// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionModifiers, PageHandler as SearchboxPageHandler, SuggestInventory} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSearchboxPageHandler extends TestBrowserProxy implements
    SearchboxPageHandler {
  constructor() {
    super([
      'queryAutocomplete',
      'openAutocompleteMatch',
    ]);
  }

  queryAutocomplete(
      queryId: number, input: string, preventInlineAutocomplete: boolean,
      cursorPosition: number, suggestInventory: SuggestInventory,
      isOnFocus: boolean) {
    this.methodCalled('queryAutocomplete', [
      queryId,
      input,
      preventInlineAutocomplete,
      cursorPosition,
      suggestInventory,
      isOnFocus,
    ]);
  }

  openAutocompleteMatch(
      line: number, url: Url, areMatchesShowing: boolean, mouseButton: number,
      modifiers: ActionModifiers, viaKeyboard: boolean) {
    this.methodCalled('openAutocompleteMatch', [
      line,
      url,
      areMatchesShowing,
      mouseButton,
      modifiers.altKey,
      modifiers.ctrlKey,
      modifiers.metaKey,
      modifiers.shiftKey,
      viaKeyboard,
    ]);
  }
}
