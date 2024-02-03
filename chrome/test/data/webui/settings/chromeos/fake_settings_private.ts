// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.settingsPrivate for testing.
 */

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';

type SettingsPrivateApi = typeof chrome.settingsPrivate;
type PrefObject = chrome.settingsPrivate.PrefObject;
type PrefType = chrome.settingsPrivate.PrefType;

const deepCopy = structuredClone;

/**
 * Fake of chrome.settingsPrivate API. Use by setting
 * CrSettingsPrefs.deferInitialization to true, then passing a
 * FakeSettingsPrivate to settings-prefs#initialize().
 */
export class FakeSettingsPrivate implements SettingsPrivateApi {
  // Mirroring chrome.settingsPrivate API members.
  /* eslint-disable @typescript-eslint/naming-convention */
  PrefType = chrome.settingsPrivate.PrefType;
  ControlledBy = chrome.settingsPrivate.ControlledBy;
  Enforcement = chrome.settingsPrivate.Enforcement;
  /* eslint-enable @typescript-eslint/naming-convention */

  prefs: Record<string, PrefObject> = {};
  onPrefsChanged = new FakeChromeEvent();
  private disallowSetPref_ = false;
  private failNextSetPref_ = false;


  constructor(initialPrefs?: PrefObject[]) {
    if (initialPrefs) {
      for (const pref of initialPrefs) {
        this.addPref_(pref.type, pref.key, pref.value);
      }
    }
  }

  // chrome.settingsPrivate overrides.
  getAllPrefs(): Promise<PrefObject[]> {
    // Send a copy of prefs to keep our internal state private.
    const prefs: PrefObject[] = [];
    for (const key in this.prefs) {
      prefs.push(deepCopy(this.prefs[key]!));
    }
    return Promise.resolve(prefs);
  }

  setPref(key: string, value: unknown, _pageId?: string): Promise<boolean> {
    const pref = this.prefs[key];
    assertTrue(!!pref);
    assertEquals(typeof value, typeof pref.value);
    assertEquals(Array.isArray(value), Array.isArray(pref.value));

    if (this.failNextSetPref_) {
      this.failNextSetPref_ = false;
      return Promise.resolve(false);
    }
    assertNotEquals(true, this.disallowSetPref_);

    const changed = JSON.stringify(pref.value) !== JSON.stringify(value);
    pref.value = deepCopy(value);

    // Like chrome.settingsPrivate, send a notification when prefs change.
    if (changed) {
      this.sendPrefChanges([{key, value: deepCopy(value)}]);
    }
    return Promise.resolve(true);
  }

  getPref(key: string): Promise<PrefObject> {
    const pref = this.prefs[key];
    assertTrue(!!pref);
    return Promise.resolve(deepCopy(pref));
  }

  // Functions used by tests.

  /** Instructs the API to return a failure when setPref is next called. */
  failNextSetPref(): void {
    this.failNextSetPref_ = true;
  }

  /** Instructs the API to assert (fail the test) if setPref is called. */
  disallowSetPref(): void {
    this.disallowSetPref_ = true;
  }

  allowSetPref(): void {
    this.disallowSetPref_ = false;
  }

  /**
   * Notifies the listeners of pref changes.
   */
  sendPrefChanges(changes: Array<{key: string, value: unknown}>): void {
    const prefs = [];
    for (const change of changes) {
      const pref = this.prefs[change.key];
      assertTrue(!!pref);
      pref.value = change.value;
      prefs.push(deepCopy(pref));
    }
    this.onPrefsChanged.callListeners(prefs);
  }

  getDefaultZoom(): Promise<number> {
    return Promise.resolve(1);
  }

  setDefaultZoom(): void {}

  // Private methods for use by the fake API.
  private addPref_(type: PrefType, key: string, value: unknown): void {
    this.prefs[key] = {
      type,
      key,
      value,
    };
  }
}
