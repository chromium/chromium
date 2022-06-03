// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of SettingsSearchHandler for testing.
 */
cr.define('settings', function() {
  /**
   * Fake implementation of chromeos.settings.mojom.SettingsSearchHandlerRemote.
   *
   * @implements {chromeos.settings.mojom.SearchHandlerInterface}
   */
  /* #export */ class FakeSettingsSearchHandler {
    constructor() {
      /** @private {!Array<chromeos.settings.mojom.SearchResult>} */
      this.fakeResults_ = [];

      /** @private {!chromeos.settings.mojom.SearchResultsObserverInterface} */
      this.observer_;
    }

    /**
     * @param {!Array<chromeos.settings.mojom.SearchResult>} results Fake
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

    simulateSearchResultAvailabilityChanged() {
      if (this.observer_) {
        this.observer_.onSearchResultAvailabilityChanged();
      }
    }
  }

  // #cr_define_end
  return {FakeSettingsSearchHandler: FakeSettingsSearchHandler};
});
