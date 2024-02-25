// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LanguageHelper, LanguageState} from 'chrome://os-settings/lazy_load.js';

export const FIRST_PARTY_INPUT_METHOD_ID_PREFIX =
    '_comp_ime_jkghodnilhceideoidjikpgommlajknk';

export class FakeLanguageHelper implements LanguageHelper {
  async whenReady(): Promise<void> {}
  setProspectiveUiLanguage(_: string): void {}
  requiresRestart(): boolean {
    return false;
  }
  getArcImeLanguageCode(): string {
    return '';
  }
  isLanguageCodeForArcIme(_: string): boolean {
    return false;
  }
  isLanguageTranslatable(_: chrome.languageSettingsPrivate.Language): boolean {
    return true;
  }
  isLanguageEnabled(_: string): boolean {
    return true;
  }
  enableLanguage(_: string): void {}
  disableLanguage(_: string): void {}
  isOnlyTranslateBlockedLanguage(_: LanguageState): boolean {
    return false;
  }
  canDisableLanguage(_: LanguageState): boolean {
    return true;
  }
  canEnableLanguage(_: chrome.languageSettingsPrivate.Language): boolean {
    return true;
  }
  moveLanguage(_1: string, _2: boolean): void {}
  moveLanguageToFront(_: string): void {}
  enableTranslateLanguage(_: string): void {}
  disableTranslateLanguage(_: string): void {}
  setLanguageAlwaysTranslateState(_1: string, _2: boolean): void {}
  toggleSpellCheck(_1: string, _2: boolean): void {}
  convertLanguageCodeForTranslate(_: string): string {
    return '';
  }
  getLanguageCodeWithoutRegion(_: string): string {
    return '';
  }
  getLanguage(_: string): chrome.languageSettingsPrivate.Language|undefined {
    return undefined;
  }
  retryDownloadDictionary(_: string): void {}
  addInputMethod(_: string): void {}
  removeInputMethod(_: string): void {}
  setCurrentInputMethod(_: string): void {}
  getInputMethodsForLanguage(_: string):
      chrome.languageSettingsPrivate.InputMethod[] {
    return [
      {
        id: 'fake display name',
        displayName: 'fake display name',
        languageCodes: ['en', 'en-US'],
        tags: [],
        enabled: true,
      },
    ];
  }
  getInputMethodsForLanguages(_: string[]):
      chrome.languageSettingsPrivate.InputMethod[] {
    return [
      {
        id: 'fake display name',
        displayName: 'fake display name',
        languageCodes: ['en', 'en-US'],
        tags: [],
        enabled: true,
      },
    ];
  }
  getEnabledLanguageCodes(): Set<string> {
    return new Set<string>();
  }
  isInputMethodEnabled(_: string): boolean {
    return true;
  }
  isComponentIme(_: chrome.languageSettingsPrivate.InputMethod): boolean {
    return false;
  }
  openInputMethodOptions(_: string): void {}
  getInputMethodDisplayName(_: string): string {
    return 'fake display name';
  }
  getCurrentInputMethod(): Promise<string> {
    return Promise.resolve(FIRST_PARTY_INPUT_METHOD_ID_PREFIX + 'xkb:us::eng');
  }
  getImeLanguagePackStatus(): chrome.inputMethodPrivate.LanguagePackStatus {
    return chrome.inputMethodPrivate.LanguagePackStatus.UNKNOWN;
  }
}
