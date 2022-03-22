// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
// clang-format on

/** @fileoverview Fake implementation of chrome.settingsPrivate for testing. */

/**
 * Creates a deep copy of the object.
 */
function deepCopy(obj: object): object {
  return JSON.parse(JSON.stringify(obj));
}

/**
 * Fake of chrome.settingsPrivate API. Use by setting
 * CrSettingsPrefs.deferInitialization to true, then passing a
 * FakeSettingsPrivate to settings-prefs#initialize().
 */
export class FakeSettingsPrivate {
  private disallowSetPref_: boolean = false;
  private failNextSetPref_: boolean = false;
  prefs: {[key: string]: chrome.settingsPrivate.PrefObject} = {};
  onPrefsChanged: FakeChromeEvent = new FakeChromeEvent();

  constructor(initialPrefs?: chrome.settingsPrivate.PrefObject[]) {
    if (!initialPrefs) {
      return;
    }
    for (const pref of initialPrefs) {
      this.addPref_(pref.type, pref.key, pref.value);
    }
  }

  // chrome.settingsPrivate overrides.
  getAllPrefs(callback: (prefs: chrome.settingsPrivate.PrefObject[]) => void) {
    // Send a copy of prefs to keep our internal state private.
    const prefs = [];
    for (const key in this.prefs) {
      prefs.push(
          deepCopy(this.prefs[key]!) as chrome.settingsPrivate.PrefObject);
    }

    // Run the callback asynchronously to test that the prefs aren't actually
    // used before they become available.
    setTimeout(callback.bind(null, prefs));
  }

  setPref(
      key: string, value: any, _pageId: string,
      callback: (param: boolean) => void) {
    const pref = this.prefs[key];
    assertNotEquals(undefined, pref);
    assertEquals(typeof value, typeof pref!.value);
    assertEquals(Array.isArray(value), Array.isArray(pref!.value));

    if (this.failNextSetPref_) {
      callback(false);
      this.failNextSetPref_ = false;
      return;
    }
    assertNotEquals(true, this.disallowSetPref_);

    const changed = JSON.stringify(pref!.value) !== JSON.stringify(value);
    pref!.value = deepCopy(value);
    callback(true);

    // Like chrome.settingsPrivate, send a notification when prefs change.
    if (changed) {
      this.sendPrefChanges([{key: key, value: deepCopy(value)}]);
    }
  }

  getPref(
      key: string,
      callback: (pref: chrome.settingsPrivate.PrefObject) => void) {
    const pref = this.prefs[key];
    assertNotEquals(undefined, pref);
    callback(deepCopy(pref!) as chrome.settingsPrivate.PrefObject);
  }

  // Functions used by tests.

  /** Instructs the API to return a failure when setPref is next called. */
  failNextSetPref() {
    this.failNextSetPref_ = true;
  }

  /** Instructs the API to assert (fail the test) if setPref is called. */
  disallowSetPref() {
    this.disallowSetPref_ = true;
  }

  allowSetPref() {
    this.disallowSetPref_ = false;
  }

  /**
   * Notifies the listeners of pref changes.
   */
  sendPrefChanges(changes: Array<{key: string, value: any}>) {
    const prefs = [];
    for (const change of changes) {
      const pref = this.prefs[change.key];
      assertNotEquals(undefined, pref);
      pref!.value = change.value;
      prefs.push(deepCopy(pref!) as chrome.settingsPrivate.PrefObject);
    }
    this.onPrefsChanged.callListeners(prefs);
  }

  getDefaultZoom() {}

  setDefaultZoom() {}

  // Private methods for use by the fake API.

  private addPref_(
      type: chrome.settingsPrivate.PrefType, key: string, value: any) {
    this.prefs[key] = {
      type: type,
      key: key,
      value: value,
    };
  }
}
