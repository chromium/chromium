// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronListElement, LanguagesBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {FakeLanguageSettingsPrivate} from '../fake_language_settings_private.js';
import {TestLanguagesBrowserProxy} from '../test_languages_browser_proxy.js';

const fakeLanguageSettingsPrivate = new FakeLanguageSettingsPrivate();
const fakeLanguagesProxy = new TestLanguagesBrowserProxy();
fakeLanguagesProxy.setLanguageSettingsPrivate(
    fakeLanguageSettingsPrivate as unknown as
    typeof chrome.languageSettingsPrivate);
LanguagesBrowserProxyImpl.setInstance(fakeLanguagesProxy);

Router.getInstance().navigateTo(routes.EDIT_DICTIONARY);
const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);

const settingsMain = settingsUi.$.main;
assertTrue(!!settingsMain);
const settingsBasicPage =
    settingsMain.shadowRoot!.querySelector('settings-basic-page');
assertTrue(!!settingsBasicPage);
const languagesPage =
    settingsBasicPage.shadowRoot!.querySelector('settings-languages-page');
assertTrue(!!languagesPage);
const dictionaryPage =
    languagesPage.shadowRoot!.querySelector('settings-edit-dictionary-page');
assertTrue(!!dictionaryPage);

fakeLanguageSettingsPrivate.addSpellcheckWord('one');
const list = dictionaryPage.shadowRoot!.querySelector<IronListElement>('#list');
assertTrue(!!list);
assertEquals(1, list.items!.length);

flush();
flushTasks().then(() => {
  document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
});
