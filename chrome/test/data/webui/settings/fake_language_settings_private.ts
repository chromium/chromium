// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.languageSettingsPrivate
 * for testing.
 */

import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Fake of the chrome.languageSettingsPrivate API.
 */
export class FakeLanguageSettingsPrivate extends TestBrowserProxy {
  private settingsPrefs_: SettingsPrefsElement|null = null;

  onSpellcheckDictionariesChanged: FakeChromeEvent;
  onCustomDictionaryChanged: FakeChromeEvent;
  onInputMethodAdded: FakeChromeEvent;
  onInputMethodRemoved: FakeChromeEvent;

  languages: chrome.languageSettingsPrivate.Language[];
  componentExtensionImes: chrome.languageSettingsPrivate.InputMethod[];

  constructor() {
    // List of method names expected to be tested with whenCalled()
    super([
      'getSpellcheckWords',
    ]);

    /**
     * Called when the pref for the dictionaries used for spell checking
     * changes or the status of one of the spell check dictionaries changes.
     */
    this.onSpellcheckDictionariesChanged = new FakeChromeEvent();

    /**
     * Called when words are added to and/or removed from the custom spell
     * check dictionary.
     */
    this.onCustomDictionaryChanged = new FakeChromeEvent();

    /**
     * Called when an input method is added.
     */
    this.onInputMethodAdded = new FakeChromeEvent();

    this.onInputMethodRemoved = new FakeChromeEvent();

    this.languages = [
      {
        // English and some variants.
        code: 'en',
        displayName: 'English',
        nativeDisplayName: 'English',
        supportsTranslate: true,
      },
      {
        code: 'en-CA',
        displayName: 'English (Canada)',
        nativeDisplayName: 'English (Canada)',
        supportsSpellcheck: true,
        supportsUI: true,
      },
      {
        code: 'en-US',
        displayName: 'English (United States)',
        nativeDisplayName: 'English (United States)',
        supportsSpellcheck: true,
        supportsUI: true,
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
      {
        // A standalone language that doesn't support anything.
        code: 'tk',
        displayName: 'Turkmen',
        nativeDisplayName: 'Turkmen',
      },
      {
        // Edge cases:
        // Norwegian is the macrolanguage for "nb" (see below).
        code: 'no',
        displayName: 'Norwegian',
        nativeDisplayName: 'norsk',
        supportsTranslate: true,
      },
      {
        // Norwegian language codes don't start with "no-" but should still
        // fall under the Norwegian macrolanguage.
        // TODO(michaelpg): Test this is ordered correctly.
        code: 'nb',
        displayName: 'Norwegian Bokmål',
        nativeDisplayName: 'norsk bokmål',
        supportsSpellcheck: true,
        supportsTranslate: true,
        supportsUI: true,
      },
      {
        // A language where displayName and nativeDisplayName have different
        // values. Used for testing search functionality.
        code: 'el',
        displayName: 'Greek',
        nativeDisplayName: 'Ελληνικά',
        supportsUI: true,
      },
      {
        // A fake language for ARC IMEs which is for internal use only. The
        // value of the |code| must be the same as |kArcImeLanguage| in
        // ui/base/ime/ash/extension_ime_util.cc.
        code: '_arc_ime_language_',
        displayName: 'Keyboard apps',
        nativeDisplayName: 'Keyboard apps',
      },
      {
        // Hebrew. This is used to test that the old language code "iw"
        // still works.
        code: 'he',
        displayName: 'Hebrew',
        nativeDisplayName: 'Hebrew',
        supportsUI: true,
      },
      {
        // Filipino. This is used to test that 'tl' is converted to 'fil'
        code: 'fil',
        displayName: 'Filipino',
        nativeDisplayName: 'Filipino',
        supportsUI: true,
      },
    ];

    this.componentExtensionImes = [
      {
        id: '_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng',
        displayName: 'US keyboard',
        languageCodes: ['en', 'en-US'],
        tags: ['US keyboard', 'English', 'English(United States)'],
        enabled: true,
      },
      {
        id: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
        displayName: 'US Dvorak keyboard',
        languageCodes: ['en', 'en-US'],
        tags: ['US Dvorak keyboard', 'English', 'English(United States)'],
        enabled: true,
      },
      {
        id: '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw',
        displayName: 'Swahili keyboard',
        languageCodes: ['sw', 'tk'],
        tags: ['Swahili keyboard', 'Swahili', 'Turkmen'],
        enabled: false,
      },
      {
        id: 'ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw',
        displayName: 'US Swahili keyboard',
        languageCodes: ['en', 'en-US', 'sw'],
        tags: [
          'US Swahili keyboard',
          'English',
          'English(United States)',
          'Swahili',
        ],
        enabled: false,
      },
      {
        id: '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:intl',
        displayName: 'US International keyboard',
        languageCodes: ['en-US'],
        tags: ['US International keyboard', 'English(United States)'],
        enabled: false,
        isProhibitedByPolicy: true,
      },
      {
        id: '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:vi:vi',
        displayName: 'Vietnamese keyboard',
        languageCodes: ['vi'],
        tags: ['Vietnamese keyboard', 'Vietnamese'],
        enabled: false,
      },
    ];
  }

  setSettingsPrefs(settingsPrefs: SettingsPrefsElement) {
    this.settingsPrefs_ = settingsPrefs;
  }

  // LanguageSettingsPrivate fake.

  /**
   * Gets languages available for translate, spell checking, input and locale.
   */
  getLanguageList() {
    return Promise.resolve(structuredClone(this.languages));
  }

  /**
   * Gets languages that should always be automatically translated.
   */
  getAlwaysTranslateLanguages() {
    const alwaysTranslateMap =
        this.settingsPrefs_!.get('prefs.translate_allowlists.value');
    return Promise.resolve(Object.keys(alwaysTranslateMap));
  }

  /**
   * Sets whether a given language should always be automatically translated.
   */
  setLanguageAlwaysTranslateState(
      languageCode: string, alwaysTranslate: boolean) {
    // Need to create a copy of the translate_allowlist object so that
    // preference observers are notified during tests.
    const alwaysTranslateMap = Object.assign(
        {}, this.settingsPrefs_!.get('prefs.translate_allowlists.value'));
    if (alwaysTranslate) {
      // The target language is not used in tests so set to 'en'.
      alwaysTranslateMap[languageCode] = 'en';
    } else {
      delete alwaysTranslateMap[languageCode];
    }
    this.settingsPrefs_!.set(
        'prefs.translate_allowlists.value', alwaysTranslateMap);
  }

  /**
   * Gets languages that should never be offered to translate.
   */
  getNeverTranslateLanguages() {
    return Promise.resolve(
        this.settingsPrefs_!.get('prefs.translate_blocked_languages.value'));
  }

  /**
   * Enables a language, adding it to the Accept-Language list (used to decide
   * which languages to translate, generate the Accept-Language header, etc.).
   */
  enableLanguage(languageCode: string) {
    let languageCodes =
        this.settingsPrefs_!.get('prefs.intl.accept_languages.value');
    const languages = languageCodes.split(',');
    if (languages.indexOf(languageCode) !== -1) {
      return;
    }
    languages.push(languageCode);
    languageCodes = languages.join(',');
    this.settingsPrefs_!.set(
        'prefs.intl.accept_languages.value', languageCodes);
  }

  /**
   * Disables a language, removing it from the Accept-Language list.
   */
  disableLanguage(languageCode: string) {
    let languageCodes =
        this.settingsPrefs_!.get('prefs.intl.accept_languages.value');
    const languages = languageCodes.split(',');
    const index = languages.indexOf(languageCode);
    if (index === -1) {
      return;
    }
    languages.splice(index, 1);
    languageCodes = languages.join(',');
    this.settingsPrefs_!.set(
        'prefs.intl.accept_languages.value', languageCodes);
  }

  /**
   * Enables/Disables translation for the given language.
   * This respectively removes/adds the language to the blocked set in the
   * preferences.
   */
  setEnableTranslationForLanguage(languageCode: string, enable: boolean) {
    const index =
        this.settingsPrefs_!.get('prefs.translate_blocked_languages.value')
            .indexOf(languageCode);
    if (enable) {
      if (index === -1) {
        return;
      }
      this.settingsPrefs_!.splice(
          'prefs.translate_blocked_languages.value', index, 1);
    } else {
      if (index !== -1) {
        return;
      }
      this.settingsPrefs_!.push(
          'prefs.translate_blocked_languages.value', languageCode);
    }
  }

  /**
   * Moves a language inside the language list.
   * Movement is determined by the |moveType| parameter.
   */
  moveLanguage(
      languageCode: string, moveType: chrome.languageSettingsPrivate.MoveType) {
    let languageCodes =
        this.settingsPrefs_!.get('prefs.intl.accept_languages.value');
    const languages = languageCodes.split(',');
    const index = languages.indexOf(languageCode);

    if (moveType === chrome.languageSettingsPrivate.MoveType.TOP) {
      if (index < 1) {
        return;
      }

      languages.splice(index, 1);
      languages.unshift(languageCode);
    } else if (moveType === chrome.languageSettingsPrivate.MoveType.UP) {
      if (index < 1) {
        return;
      }

      const temp = languages[index - 1];
      languages[index - 1] = languageCode;
      languages[index] = temp;
    } else if (moveType === chrome.languageSettingsPrivate.MoveType.DOWN) {
      if (index === -1 || index === languages.length - 1) {
        return;
      }

      const temp = languages[index + 1];
      languages[index + 1] = languageCode;
      languages[index] = temp;
    }

    languageCodes = languages.join(',');
    this.settingsPrefs_!.set(
        'prefs.intl.accept_languages.value', languageCodes);
  }

  /**
   * Gets the translate target language (in most cases, the display locale).
   */
  getTranslateTargetLanguage() {
    return Promise.resolve('en');
  }

  /**
   * Sets the translate target language.
   */
  setTranslateTargetLanguage(languageCode: string) {
    this.settingsPrefs_!.set(
        'prefs.translate_recent_target.value', languageCode);
  }

  /**
   * Gets the current status of the chosen spell check dictionaries.
   */
  getSpellcheckDictionaryStatuses() {
    return Promise.resolve([]);
  }

  /**
   * Gets the custom spell check words, in sorted order.
   */
  getSpellcheckWords() {
    this.methodCalled('getSpellcheckWords');
    return Promise.resolve([]);
  }

  /**
   * Adds a word to the custom dictionary.
   */
  addSpellcheckWord(word: string) {
    this.onCustomDictionaryChanged.callListeners([word], []);
  }

  /**
   * Removes a word from the custom dictionary.
   */
  removeSpellcheckWord(word: string) {
    this.onCustomDictionaryChanged.callListeners([], [word]);
  }

  /**
   * Tries to download the dictionary after a failed download.
   */
  retryDownloadDictionary(languageCode: string) {
    this.onSpellcheckDictionariesChanged.callListeners([
      {languageCode, isReady: false, isDownlading: true},
    ]);
    this.onSpellcheckDictionariesChanged.callListeners([
      {languageCode, isReady: false, downloadFailed: true},
    ]);
  }
}

// List of language-related preferences suitable for testing.
export function getFakeLanguagePrefs() {
  return [
    {
      key: 'browser.enable_spellchecking',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'intl.app_locale',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en-US',
    },
    {
      key: 'intl.accept_languages',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en-US,sw',
    },
    {
      key: 'intl.forced_languages',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: '',
    },
    {
      key: 'spellcheck.blocked_dictionaries',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [],
    },
    {
      key: 'spellcheck.dictionaries',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: ['en-US'],
    },
    {
      key: 'spellcheck.forced_dictionaries',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [],
    },
    {
      key: 'spellcheck.use_spelling_service',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'translate.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'translate_blocked_languages',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: ['en-US'],
    },
    {
      key: 'translate_site_blocklist_with_time',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {
        'ru.wikipedia.org': '13305315102292953',
        'de.wikipedia.org': '13305315083099649',
      },
    },
    // Note: The real implementation of this pref is actually a dictionary
    // of {always translate: target}, however only the keys are needed for
    // testing so target will always be 'en'.
    {
      key: 'translate_allowlists',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {},
    },
    {
      key: 'translate_recent_target',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en',
    },
  ];
}
