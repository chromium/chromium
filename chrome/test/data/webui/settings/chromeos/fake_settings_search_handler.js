// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of SettingsSearchHandler for testing.
 */
/**
 * Fake implementation of chromeos.settings.mojom.SettingsSearchHandlerRemote.
 *
 * @implements {ash.settings.mojom.SearchHandlerInterface}
 */
export class FakeSettingsSearchHandler {
  constructor() {
    /** @private {!Array<ash.settings.mojom.SearchResult>} */
    this.fakeResults_ = [];

    /** @private {!ash.settings.mojom.SearchResultsObserverInterface} */
    this.observer_;
  }

  /**
   * @param {!Array<ash.settings.mojom.SearchResult>} results Fake
   *     results that will be returned when Search() is called.
   */
  setFakeResults(results) {
    this.fakeResults_ = results;
  }

  /** override */
  async search(query, maxNumResults, parentResultBehavior) {
    return {results: this.fakeResults_};
  }

  /** override */
  observe(observer) {
    this.observer_ = observer;
  }

  simulateSearchResultsChanged() {
    if (this.observer_) {
      this.observer_.onSearchResultsChanged();
    }
  }
}
