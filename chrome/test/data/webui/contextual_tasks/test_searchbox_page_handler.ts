// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandler as SearchboxPageHandler} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
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

  queryAutocomplete(input: string, preventInlineAutocomplete: boolean) {
    this.methodCalled('queryAutocomplete', [input, preventInlineAutocomplete]);
  }

  openAutocompleteMatch(
      line: number, url: Url, areMatchesShowing: boolean, mouseButton: number,
      altKey: boolean, ctrlKey: boolean, metaKey: boolean, shiftKey: boolean) {
    this.methodCalled('openAutocompleteMatch', [
      line,
      url,
      areMatchesShowing,
      mouseButton,
      altKey,
      ctrlKey,
      metaKey,
      shiftKey,
    ]);
  }
}
