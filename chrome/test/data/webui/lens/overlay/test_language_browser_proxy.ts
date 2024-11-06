// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguageBrowserProxyImpl} from 'chrome-untrusted://lens-overlay/language_browser_proxy.js';
import type {Language} from 'chrome-untrusted://lens-overlay/translate.mojom-webui.js';

/**
 * Test version of the LanguageBrowserProxy used in connecting Lens Overlay to
 * the language settings API.
 */
export class TestLanguageBrowserProxy extends LanguageBrowserProxyImpl {
  private storedLocale: string = '';
  private storedSourceLanguages: Language[] = [];
  private storedTargetLanguages: Language[] = [];

  override getClientLanguageList(): Promise<Language[]> {
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

  override getTranslateTargetLanguage(): Promise<string> {
    return Promise.resolve('en');
  }

  override storeLanguages(
      locale: string, sourceLanguages: Language[],
      targetLanguages: Language[]): void {
    this.storedLocale = locale;
    this.storedSourceLanguages = sourceLanguages;
    this.storedTargetLanguages = targetLanguages;
    // We need to call the original function so that the timestamp is properly
    // set.
    super.storeLanguages(locale, sourceLanguages, targetLanguages);
  }

  getStoredLocale(): string {
    return this.storedLocale;
  }

  getStoredSourceLanguages(): Language[] {
    return this.storedSourceLanguages;
  }

  getStoredTargetLanguages(): Language[] {
    return this.storedTargetLanguages;
  }
}
