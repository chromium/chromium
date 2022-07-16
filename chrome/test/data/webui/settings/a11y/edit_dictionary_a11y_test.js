// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate} from '../fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from '../test_languages_browser_proxy.js';

const fakeLanguageSettingsPrivate = new FakeLanguageSettingsPrivate();
const fakeLanugagesProxy = new TestLanguagesBrowserProxy();
fakeProxy.setLanguageSettingsPrivate(fakeLangugageSettingsPrivate);

Router.getInstance().navigateTo(settings.routes.EDIT_DICTIONARY);
const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);

const settingsMain = settingsUI.$.main;
assertTrue(!!settingsMain);
const settingsBasicPage =
    settingsMain.shadowRoot.querySelector('settings-basic-page');
assertTrue(!!settingsBasicPage);
const languagesPage =
    settingsBasicPage.shadowRoot.querySelector('settings-languages-page');
assertTrue(!!languagesPage);
const dictionaryPage =
    languagesPage.shadowRoot.querySelector('settings-edit-dictionary-page');
assertTrue(!!dictionaryPage);

fakeLanguageSettingsPrivate.addSpellcheckWord('one');
assertTrue(!!dictionaryPage.shadowRoot.querySelector('#list'));
assertEquals(1, dictionaryPage.shadowRoot.querySelector('#list').items.length);

flush();
flushTasks().then(() => {
  document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
});
