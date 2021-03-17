// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {FakeLanguageSettingsPrivate} from 'chrome://test/settings/fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from 'chrome://test/settings/test_languages_browser_proxy.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

const fakeLanguageSettingsPrivate = new FakeLanguageSettingsPrivate();
const fakeLanugagesProxy = new TestLanguagesBrowserProxy();
fakeProxy.setLanguageSettingsPrivate(fakeLangugageSettingsPrivate);

Router.getInstance().navigateTo(settings.routes.EDIT_DICTIONARY);
const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);

const settingsMain = settingsUI.$.main;
assertTrue(!!settingsMain);
const settingsBasicPage = settingsMain.$$('settings-basic-page');
assertTrue(!!settingsBasicPage);
const languagesPage = settingsBasicPage.$$('settings-languages-page');
assertTrue(!!languagesPage);
const dictionaryPage = languagesPage.$$('settings-edit-dictionary-page');
assertTrue(!!dictionaryPage);

fakeLanguageSettingsPrivate.addSpellcheckWord('one');
assertTrue(!!dictionaryPage.$$('#list'));
assertEquals(1, dictionaryPage.$$('#list').items.length);

flush();
flushTasks().then(() => {
  document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
});
