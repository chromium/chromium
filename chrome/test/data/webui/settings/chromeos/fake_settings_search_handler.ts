// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of SettingsSearchHandler for testing.
 */

import {searchMojom} from 'chrome://os-settings/os_settings.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

type SearchResult = searchMojom.SearchResult;
type SearchHandlerInterface = searchMojom.SearchHandlerInterface;
type SearchResultsObserverInterface =
    searchMojom.SearchResultsObserverInterface;

/**
 * Fake implementation of chromeos.settings.mojom.SettingsSearchHandlerRemote.
 */
export class FakeSettingsSearchHandler implements SearchHandlerInterface {
  private fakeResults_: SearchResult[] = [];
  private observer_: SearchResultsObserverInterface|null = null;

  setFakeResults(results: SearchResult[]): void {
    this.fakeResults_ = results;
  }

  async search(_query: String16, _maxNumResults: number):
      Promise<{results: SearchResult[]}> {
    return {results: this.fakeResults_};
  }

  observe(observer: SearchResultsObserverInterface): void {
    this.observer_ = observer;
  }

  simulateSearchResultsChanged(): void {
    if (this.observer_) {
      this.observer_.onSearchResultsChanged();
    }
  }
}
