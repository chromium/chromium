// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LanguageBrowserProxy} from 'chrome-untrusted://lens-overlay/language_browser_proxy.js';

/**
 * Test version of the LanguageBrowserProxy used in connecting Lens Overlay to
 * the language settings API.
 */
export class TestLanguageBrowserProxy implements LanguageBrowserProxy {
  getLanguageList(): Promise<chrome.languageSettingsPrivate.Language[]> {
    return Promise.resolve(structuredClone([
      {
        // English language.
        code: 'en',
        displayName: 'English',
        nativeDisplayName: 'English',
        supportsTranslate: true,
      },
      {
        // A standalone language.
        code: 'sw',
        displayName: 'Swahili',
        nativeDisplayName: 'Kiswahili',
        supportsSpellcheck: true,
        supportsTranslate: true,
        supportsUI: true,
      },
    ]));
  }

  getTranslateTargetLanguage(): Promise<string> {
    return Promise.resolve('en');
  }
}
