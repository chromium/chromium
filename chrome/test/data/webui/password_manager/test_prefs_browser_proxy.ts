// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PrefsBrowserProxy. */

import {assert} from '//resources/js/assert_ts.js';
import {PrefsBrowserProxy, PrefsChangedListener} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test implementation
 */
export class TestPrefsBrowserProxy extends TestBrowserProxy implements
    PrefsBrowserProxy {
  prefs: chrome.settingsPrivate.PrefObject[];
  listeners: PrefsChangedListener[];

  constructor() {
    super([
      'getPref',
      'setPref',
    ]);

    this.prefs = [];
    this.listeners = [];
  }

  addPrefsChangedListener(listener: PrefsChangedListener) {
    this.listeners.push(listener);
  }

  removePrefsChangedListener(listener: PrefsChangedListener) {
    const index = this.listeners.indexOf(listener);
    assert(index !== -1);
    this.listeners.splice(index, 1);
  }

  getPref(key: string): Promise<chrome.settingsPrivate.PrefObject> {
    this.methodCalled('getPref', {key});
    const pref = this.prefs.find(pref => pref.key === key);
    assert(pref);
    return Promise.resolve(pref);
  }

  setPref(key: string, value: any): Promise<boolean> {
    this.methodCalled('setPref', {key, value});
    const index = this.prefs.findIndex((pref => pref.key === key));
    this.prefs[index]!.value = value;
    this.notifyListeners_();
    return Promise.resolve(true);
  }

  private notifyListeners_() {
    this.listeners.forEach(listener => listener(this.prefs.slice()));
  }
}
