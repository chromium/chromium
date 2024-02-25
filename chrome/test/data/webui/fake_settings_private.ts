// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertEquals, assertNotEquals} from './chai_assert.js';
import {TestBrowserProxy} from './test_browser_proxy.js';
import {FakeChromeEvent} from './fake_chrome_event.js';
// clang-format on

/** @fileoverview Fake implementation of chrome.settingsPrivate for testing. */

type SettingsPrivateApi = typeof chrome.settingsPrivate;
type PrefObject = chrome.settingsPrivate.PrefObject;
type PrefType = chrome.settingsPrivate.PrefType;

/**
 * Fake of chrome.settingsPrivate API. Use by setting
 * CrSettingsPrefs.deferInitialization to true, then passing a
 * FakeSettingsPrivate to settings-prefs#initialize().
 */
export class FakeSettingsPrivate extends TestBrowserProxy implements
    SettingsPrivateApi {
  // Mirroring chrome.settingsPrivate API members.
  /* eslint-disable @typescript-eslint/naming-convention */
  PrefType = chrome.settingsPrivate.PrefType;
  ControlledBy = chrome.settingsPrivate.ControlledBy;
  Enforcement = chrome.settingsPrivate.Enforcement;
  /* eslint-enable @typescript-eslint/naming-convention */

  prefs: Record<string, PrefObject> = {};
  onPrefsChanged: FakeChromeEvent = new FakeChromeEvent();
  private disallowSetPref_: boolean = false;
  private failNextSetPref_: boolean = false;

  constructor(initialPrefs?: PrefObject[]) {
    super([
      'setPref',
      'getPref',
    ]);

    if (!initialPrefs) {
      return;
    }
    for (const pref of initialPrefs) {
      this.addPref_(pref.type, pref.key, pref.value);
    }
  }

  // chrome.settingsPrivate overrides.
  getAllPrefs(): Promise<PrefObject[]> {
    // Send a copy of prefs to keep our internal state private.
    const prefs = [];
    for (const key in this.prefs) {
      prefs.push(structuredClone(this.prefs[key]!));
    }
    return Promise.resolve(prefs);
  }

  setPref(key: string, value: any, _pageId?: string): Promise<boolean> {
    this.methodCalled('setPref', {key, value});
    const pref = this.prefs[key];
    assertNotEquals(undefined, pref);
    assertEquals(typeof value, typeof pref!.value);
    assertEquals(Array.isArray(value), Array.isArray(pref!.value));

    if (this.failNextSetPref_) {
      this.failNextSetPref_ = false;
      return Promise.resolve(false);
    }
    assertNotEquals(true, this.disallowSetPref_);

    const changed = JSON.stringify(pref!.value) !== JSON.stringify(value);
    pref!.value = structuredClone(value);
    // Like chrome.settingsPrivate, send a notification when prefs change.
    if (changed) {
      this.sendPrefChanges([{key: key, value: structuredClone(value)}]);
    }
    return Promise.resolve(true);
  }

  getPref(key: string): Promise<PrefObject> {
    this.methodCalled('getPref', key);
    const pref = this.prefs[key];
    assertNotEquals(undefined, pref);
    return Promise.resolve(structuredClone(pref!));
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
      prefs.push(structuredClone(pref!) as PrefObject);
    }
    this.onPrefsChanged.callListeners(prefs);
  }

  getDefaultZoom(): Promise<number> {
    return Promise.resolve(100);
  }

  setDefaultZoom(): void {}

  // Private methods for use by the fake API.

  private addPref_(type: PrefType, key: string, value: any) {
    this.prefs[key] = {
      type: type,
      key: key,
      value: value,
    };
  }
}
