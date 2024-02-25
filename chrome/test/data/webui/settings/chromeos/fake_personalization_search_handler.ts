// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of Personalization SearchHandler for
 * testing.
 */

import {personalizationSearchMojom} from 'chrome://os-settings/os_settings.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

type SearchResult = personalizationSearchMojom.SearchResult;
type SearchHandlerInterface = personalizationSearchMojom.SearchHandlerInterface;
type SearchResultsObserverInterface =
    personalizationSearchMojom.SearchResultsObserverInterface;

export class FakePersonalizationSearchHandler implements
    SearchHandlerInterface {
  private fakeResults_: SearchResult[] = [];
  private observer_: SearchResultsObserverInterface|null = null;

  setFakeResults(results: SearchResult[]): void {
    this.fakeResults_ = results;
  }

  async search(_query: String16, _maxNumResults: number):
      Promise<{results: SearchResult[]}> {
    return {results: this.fakeResults_};
  }

  addObserver(observer: SearchResultsObserverInterface): void {
    this.observer_ = observer;
  }

  simulateSearchResultsChanged(): void {
    if (this.observer_) {
      this.observer_.onSearchResultsChanged();
    }
  }
}
