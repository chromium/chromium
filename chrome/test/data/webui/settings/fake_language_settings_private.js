// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.languageSettingsPrivate
 * for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.languageSettingsPrivate API.
   * @implements {LanguageSettingsPrivate}
   */
  class FakeLanguageSettingsPrivate extends TestBrowserProxy {
    constructor() {
      // List of method names expected to be tested with whenCalled()
      super([
        'getSpellcheckWords',
      ]);

      /**
       * Called when the pref for the dictionaries used for spell checking
       * changes or the status of one of the spell check dictionaries changes.
       * @type {ChromeEvent}
       */
      this.onSpellcheckDictionariesChanged = new FakeChromeEvent();

      /**
       * Called when words are added to and/or removed from the custom spell
       * check dictionary.
       * @type {ChromeEvent}
       */
      this.onCustomDictionaryChanged = new FakeChromeEvent();

      /**
       * Called when an input method is added.
       * @type {ChromeEvent}
       */
      this.onInputMethodAdded = new FakeChromeEvent();

      /**
       * Called when an input method is removed.
       * @type {ChromeEvent}
       */
      this.onInputMethodRemoved = new FakeChromeEvent();

      /** @type {!Array<!chrome.languageSettingsPrivate.Language>} */
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
          nativeDisplayName: 'Turkmen'
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
          // ui/base/ime/chromeos/extension_ime_util.cc.
          code: '_arc_ime_language_',
          displayName: 'Keyboard apps',
        }
      ];

      /** @type {!Array<!chrome.languageSettingsPrivate.InputMethod>} */
      this.componentExtensionImes = [
        {
          id: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng',
          displayName: 'US keyboard',
          languageCodes: ['en', 'en-US'],
          enabled: true,
        },
        {
          id: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
          displayName: 'US Dvorak keyboard',
          languageCodes: ['en', 'en-US'],
          enabled: true,
        },
        {
          id: '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:sw:sw',
          displayName: 'Swahili keyboard',
          languageCodes: ['sw', 'tk'],
          enabled: false,
        },
        {
          id: '_comp_ime_abcdefghijklmnopqrstuvwxyzabcdefxkb:us:sw',
          displayName: 'US Swahili keyboard',
          languageCodes: ['en', 'en-US', 'sw'],
          enabled: false,
        }
      ];
    }

    /** @param {SettingsPrefsElement} */
    setSettingsPrefs(settingsPrefs) {
      this.settingsPrefs_ = settingsPrefs;
    }

    // LanguageSettingsPrivate fake.

    /**
     * Gets languages available for translate, spell checking, input and locale.
     * @param {function(!Array<!chrome.languageSettingsPrivate.Language>)}
     *     callback
     */
    getLanguageList(callback) {
      setTimeout(function() {
        callback(JSON.parse(JSON.stringify(this.languages)));
      }.bind(this));
    }

    /**
     * Enables a language, adding it to the Accept-Language list (used to decide
     * which languages to translate, generate the Accept-Language header, etc.).
     * @param {string} languageCode
     */
    enableLanguage(languageCode) {
      let languageCodes = this.settingsPrefs_.prefs.intl.accept_languages.value;
      const languages = languageCodes.split(',');
      if (languages.indexOf(languageCode) != -1) {
        return;
      }
      languages.push(languageCode);
      languageCodes = languages.join(',');
      this.settingsPrefs_.set(
          'prefs.intl.accept_languages.value', languageCodes);
      if (cr.isChromeOS) {
        this.settingsPrefs_.set(
            'prefs.settings.language.preferred_languages.value', languageCodes);
      }
    }

    /**
     * Disables a language, removing it from the Accept-Language list.
     * @param {string} languageCode
     */
    disableLanguage(languageCode) {
      let languageCodes = this.settingsPrefs_.prefs.intl.accept_languages.value;
      const languages = languageCodes.split(',');
      const index = languages.indexOf(languageCode);
      if (index == -1) {
        return;
      }
      languages.splice(index, 1);
      languageCodes = languages.join(',');
      this.settingsPrefs_.set(
          'prefs.intl.accept_languages.value', languageCodes);
      if (cr.isChromeOS) {
        this.settingsPrefs_.set(
            'prefs.settings.language.preferred_languages.value', languageCodes);
      }
    }

    /**
     * Enables/Disables translation for the given language.
     * This respectively removes/adds the language to the blocked set in the
     * preferences.
     * @param {string} languageCode
     * @param {boolean} enable
     */
    setEnableTranslationForLanguage(languageCode, enable) {
      const index =
          this.settingsPrefs_.prefs.translate_blocked_languages.value.indexOf(
              languageCode);
      if (enable) {
        if (index == -1) {
          return;
        }
        this.settingsPrefs_.splice(
            'prefs.translate_blocked_languages.value', index, 1);
      } else {
        if (index != -1) {
          return;
        }
        this.settingsPrefs_.push(
            'prefs.translate_blocked_languages.value', languageCode);
      }
    }

    /**
     * Moves a language inside the language list.
     * Movement is determined by the |moveType| parameter.
     * @param {string} languageCode
     * @param {chrome.languageSettingsPrivate.MoveType} moveType
     */
    moveLanguage(languageCode, moveType) {
      let languageCodes = this.settingsPrefs_.prefs.intl.accept_languages.value;
      const languages = languageCodes.split(',');
      const index = languages.indexOf(languageCode);

      if (moveType == chrome.languageSettingsPrivate.MoveType.TOP) {
        if (index < 1) {
          return;
        }

        languages.splice(index, 1);
        languages.unshift(languageCode);
      } else if (moveType == chrome.languageSettingsPrivate.MoveType.UP) {
        if (index < 1) {
          return;
        }

        const temp = languages[index - 1];
        languages[index - 1] = languageCode;
        languages[index] = temp;
      } else if (moveType == chrome.languageSettingsPrivate.MoveType.DOWN) {
        if (index == -1 || index == languages.length - 1) {
          return;
        }

        const temp = languages[index + 1];
        languages[index + 1] = languageCode;
        languages[index] = temp;
      }

      languageCodes = languages.join(',');
      this.settingsPrefs_.set(
          'prefs.intl.accept_languages.value', languageCodes);
      if (cr.isChromeOS) {
        this.settingsPrefs_.set(
            'prefs.settings.language.preferred_languages.value', languageCodes);
      }
    }

    /**
     * Gets the current status of the chosen spell check dictionaries.
     * @param {function(!Array<
     *     !chrome.languageSettingsPrivate.SpellcheckDictionaryStatus>):void}
     *     callback
     */
    getSpellcheckDictionaryStatuses(callback) {
      callback([]);
    }

    /**
     * Gets the custom spell check words, in sorted order.
     * @param {function(!Array<string>):void} callback
     */
    getSpellcheckWords(callback) {
      callback([]);
      this.methodCalled('getSpellcheckWords');
    }

    /**
     * Adds a word to the custom dictionary.
     * @param {string} word
     */
    addSpellcheckWord(word) {
      this.onCustomDictionaryChanged.callListeners([word], []);
    }

    /**
     * Removes a word from the custom dictionary.
     * @param {string} word
     */
    removeSpellcheckWord(word) {
      this.onCustomDictionaryChanged.callListeners([], [word]);
    }

    /**
     * Gets the translate target language (in most cases, the display locale).
     * @param {function(string):void} callback
     */
    getTranslateTargetLanguage(callback) {
      setTimeout(callback.bind(null, 'en'));
    }

    /**
     * Gets all supported input methods, including third-party IMEs. Chrome OS
     * only.
     * @param {function(!chrome.languageSettingsPrivate.InputMethodLists):void}
     *     callback
     */
    getInputMethodLists(callback) {
      if (!cr.isChromeOS) {
        assertNotReached();
      }
      callback({
        componentExtensionImes:
            JSON.parse(JSON.stringify(this.componentExtensionImes)),
        thirdPartyExtensionImes: [],
      });
    }

    /**
     * Adds the input method to the current user's list of enabled input
     * methods, enabling the input method for the current user. Chrome OS only.
     * @param {string} inputMethodId
     */
    addInputMethod(inputMethodId) {
      assert(cr.isChromeOS);
      const inputMethod = this.componentExtensionImes.find(function(ime) {
        return ime.id == inputMethodId;
      });
      assertTrue(!!inputMethod);
      inputMethod.enabled = true;
      const prefPath = 'prefs.settings.language.preload_engines.value';
      const enabledInputMethods = this.settingsPrefs_.get(prefPath).split(',');
      enabledInputMethods.push(inputMethodId);
      this.settingsPrefs_.set(prefPath, enabledInputMethods.join(','));
    }

    /**
     * Removes the input method from the current user's list of enabled input
     * methods, disabling the input method for the current user. Chrome OS only.
     * @param {string} inputMethodId
     */
    removeInputMethod(inputMethodId) {
      assert(cr.isChromeOS);
      const inputMethod = this.componentExtensionImes.find(function(ime) {
        return ime.id == inputMethodId;
      });
      assertTrue(!!inputMethod);
      inputMethod.enabled = false;
      this.settingsPrefs_.set(
          'prefs.settings.language.preload_engines.value',
          this.settingsPrefs_.prefs.settings.language.preload_engines.value
              .replace(inputMethodId, ''));
    }

    /**
     * Tries to download the dictionary after a failed download.
     * @param {string} languageCode
     */
    retryDownloadDictionary(languageCode) {
      this.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: false, isDownlading: true},
      ]);
      this.onSpellcheckDictionariesChanged.callListeners([
        {languageCode, isReady: false, downloadFailed: true},
      ]);
    }
  }

  // List of language-related preferences suitable for testing.
  function getFakeLanguagePrefs() {
    const fakePrefs = [
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
        key: 'spellcheck.blacklisted_dictionaries',
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
      }
    ];
    if (cr.isChromeOS) {
      fakePrefs.push({
        key: 'settings.language.preferred_languages',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'en-US,sw',
      });
      fakePrefs.push({
        key: 'settings.language.preload_engines',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us::eng,' +
            '_comp_ime_fgoepimhcoialccpbmpnnblemnepkkaoxkb:us:dvorak:eng',
      });
      fakePrefs.push({
        key: 'settings.language.enabled_extension_imes',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '',
      });
      fakePrefs.push({
        key: 'settings.language.ime_menu_activated',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      });
    }
    return fakePrefs;
  }
  return {
    FakeLanguageSettingsPrivate: FakeLanguageSettingsPrivate,
    getFakeLanguagePrefs: getFakeLanguagePrefs,
  };
});
