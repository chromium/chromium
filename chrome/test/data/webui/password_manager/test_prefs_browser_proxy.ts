// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrefsBrowserProxy} from 'chrome://password-manager/password_manager.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';

export class TestPrefsBrowserProxy implements PrefsBrowserProxy {
  fakeApi: FakeSettingsPrivate;

  constructor(initialPrefs: chrome.settingsPrivate.PrefObject[]) {
    this.fakeApi = new FakeSettingsPrivate(initialPrefs);
  }

  getAllPrefs() {
    return this.fakeApi.getAllPrefs();
  }

  getPref(name: string) {
    return this.fakeApi.getPref(name);
  }

  setPref(name: string, value: unknown, pageId?: string) {
    return this.fakeApi.setPref(name, value, pageId);
  }

  get onPrefsChanged() {
    return this.fakeApi.onPrefsChanged as unknown as
        typeof chrome.settingsPrivate.onPrefsChanged;
  }
}
