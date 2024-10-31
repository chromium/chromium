// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LanguageBrowserProxy} from 'chrome-untrusted://lens-overlay/language_browser_proxy.js';
import type {Language} from 'chrome-untrusted://lens-overlay/translate.mojom-webui.js';

/**
 * Test version of the LanguageBrowserProxy used in connecting Lens Overlay to
 * the language settings API.
 */
export class TestLanguageBrowserProxy implements LanguageBrowserProxy {
  getLanguageList(): Promise<Language[]> {
    return Promise.resolve(structuredClone([
      {
        // English language.
        languageCode: 'en',
        name: 'English',
      },
      {
        // A standalone language.
        languageCode: 'sw',
        name: 'Swahili',
      },
    ]));
  }

  getTranslateTargetLanguage(): Promise<string> {
    return Promise.resolve('en');
  }
}
