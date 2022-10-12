// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CookieDetails, LocalDataBrowserProxy, LocalDataItem} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of LocalDataBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestLocalDataBrowserProxy extends TestBrowserProxy implements
    LocalDataBrowserProxy {
  private cookieDetails_: CookieDetails[] = [];
  private cookieList_: LocalDataItem[] = [];
  private filteredCookieList_: LocalDataItem[] = [];

  constructor() {
    super([
      'getDisplayList',
      'removeAll',
      'removeShownItems',
      'removeItem',
      'removeSite',
      'getCookieDetails',
      'getNumCookiesString',
      'getFpsMembershipLabel',
      'reloadCookies',
      'removeCookie',
      'removeAllThirdPartyCookies',
    ]);
  }

  setCookieDetails(cookieDetails: CookieDetails[]) {
    this.cookieDetails_ = cookieDetails;
  }

  setCookieList(cookieList: LocalDataItem[]) {
    this.cookieList_ = cookieList;
    this.filteredCookieList_ = cookieList;
  }

  getDisplayList(filter: string) {
    this.methodCalled('getDisplayList', filter);
    if (filter === undefined) {
      filter = '';
    }
    /** @type {!Array<!LocalDataItem>} */
    const output: LocalDataItem[] = [];
    for (let i = 0; i < this.cookieList_.length; ++i) {
      if (this.cookieList_[i]!.site.indexOf(filter) >= 0) {
        output.push(this.filteredCookieList_[i]!);
      }
    }
    return Promise.resolve(output);
  }

  removeAll() {
    this.methodCalled('removeAll');
    return Promise.resolve();
  }

  removeShownItems() {
    this.methodCalled('removeShownItems');
  }

  removeSite(site: string) {
    this.methodCalled('removeSite', site);
  }

  getCookieDetails(site: string) {
    this.methodCalled('getCookieDetails', site);
    return Promise.resolve(this.cookieDetails_);
  }

  getNumCookiesString(numCookies: number) {
    this.methodCalled('getNumCookiesString', numCookies);
    return Promise.resolve(
        `${numCookies} ` + (numCookies === 1 ? 'cookie' : 'cookies'));
  }

  getFpsMembershipLabel(fpsNumMembers: number, fpsOwner: string) {
    this.methodCalled('getFpsMembershipLabel', fpsNumMembers, fpsOwner);
    return Promise.resolve([
      'Allowed for',
      `${fpsNumMembers}`,
      `${fpsOwner}`,
      (fpsNumMembers === 1 ? 'site' : 'sites'),
    ].join(' '));
  }

  reloadCookies() {
    this.methodCalled('reloadCookies');
    return Promise.resolve();
  }

  removeItem(path: string) {
    this.methodCalled('removeItem', path);
  }

  removeAllThirdPartyCookies() {
    this.methodCalled('removeAllThirdPartyCookies');
    return Promise.resolve();
  }
}
