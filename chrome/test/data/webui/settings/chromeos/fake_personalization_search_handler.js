// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of Personalization SearchHandler for
 * testing.
 */

import {personalizationSearchMojom} from 'chrome://os-settings/chromeos/os_settings.js';

/**
 * @implements {personalizationSearchMojom.SearchHandlerInterface}
 */
export class FakePersonalizationSearchHandler {
  constructor() {
    /** @private {!Array<personalizationSearchMojom.SearchResult>} */
    this.fakeResults_ = [];

    /**
     * @private {!personalizationSearchMojom.SearchResultsObserverInterface}
     */
    this.observer_;
  }

  /**
   * @param {!Array<personalizationSearchMojom.SearchResult>} results Fake
   *     results that will be returned when Search() is called.
   */
  setFakeResults(results) {
    this.fakeResults_ = results;
  }

  /** override */
  async search(query, maxNumResults) {
    return {results: this.fakeResults_};
  }

  /** override */
  addObserver(observer) {
    this.observer_ = observer;
  }

  simulateSearchResultsChanged() {
    if (this.observer_) {
      this.observer_.onSearchResultsChanged();
    }
  }
}
