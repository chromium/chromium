// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import type {CountryDetailManagerProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type AddressComponents = chrome.autofillPrivate.AddressComponents;
type CountryEntry = chrome.autofillPrivate.CountryEntry;

export class TestCountryDetailManagerProxy extends TestBrowserProxy implements
    CountryDetailManagerProxy {
  private countryList_: CountryEntry[] = [];
  private addressComponents_: AddressComponents|null = null;

  constructor() {
    super([
      'getCountryList',
      'getAddressFormat',
    ]);
  }

  setGetCountryListRepsonse(countryList: CountryEntry[]): void {
    this.countryList_ = countryList;
  }

  setGetAddressFormatRepsonse(addressComponents: AddressComponents): void {
    this.addressComponents_ = addressComponents;
  }

  getCountryList(_forAccountStorage: boolean): Promise<CountryEntry[]> {
    return Promise.resolve(structuredClone(this.countryList_));
  }

  getAddressFormat(_countryCode: string): Promise<AddressComponents> {
    assert(this.addressComponents_);
    return Promise.resolve(structuredClone(this.addressComponents_));
  }
}
