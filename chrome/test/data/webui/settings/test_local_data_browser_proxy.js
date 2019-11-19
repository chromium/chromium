// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A test version of LocalDataBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {settings.LocalDataBrowserProxy}
 */
class TestLocalDataBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDisplayList',
      'removeAll',
      'removeShownItems',
      'removeItem',
      'getCookieDetails',
      'getNumCookiesString',
      'reloadCookies',
      'removeCookie',
      'removeThirdPartyCookies',
    ]);

    /** @private {?CookieList} */
    this.cookieDetails_ = null;

    /** @private {Array<!CookieList>} */
    this.cookieList_ = [];
  }

  /**
   * Test-only helper.
   * @param {!CookieList} cookieDetails
   */
  setCookieDetails(cookieDetails) {
    this.cookieDetails_ = cookieDetails;
  }

  /**
   * Test-only helper.
   * @param {!CookieList} cookieList
   */
  setCookieList(cookieList) {
    this.cookieList_ = cookieList;
    this.filteredCookieList_ = cookieList;
  }

  /** @override */
  getDisplayList(filter) {
    if (filter === undefined) {
      filter = '';
    }
    const output = [];
    for (let i = 0; i < this.cookieList_.length; ++i) {
      if (this.cookieList_[i].site.indexOf(filter) >= 0) {
        output.push(this.filteredCookieList_[i]);
      }
    }
    return Promise.resolve({items: output});
  }

  /** @override */
  removeAll() {
    this.methodCalled('removeAll');
    return Promise.resolve({id: null, children: []});
  }

  /** @override */
  removeShownItems() {
    this.methodCalled('removeShownItems');
  }

  /** @override */
  removeItem(id) {
    this.methodCalled('removeItem', id);
  }

  /** @override */
  getCookieDetails(site) {
    this.methodCalled('getCookieDetails', site);
    return Promise.resolve(this.cookieDetails_ || {id: '', children: []});
  }

  /** @override */
  getNumCookiesString(numCookies) {
    this.methodCalled('getNumCookiesString', numCookies);
    return Promise.resolve(
        `${numCookies} ` + (numCookies == 1 ? 'cookie' : 'cookies'));
  }

  /** @override */
  reloadCookies() {
    this.methodCalled('reloadCookies');
    return Promise.resolve({id: null, children: []});
  }

  /** @override */
  removeCookie(path) {
    this.methodCalled('removeCookie', path);
  }

  /** @override */
  removeThirdPartyCookies() {
    this.methodCalled('removeThirdPartyCookies');
  }
}
