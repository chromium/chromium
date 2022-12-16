// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CheckupSubpage, Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeInsecureCredential} from './test_util.js';

suite('CheckupDetailsSectionTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
    Router.getInstance().navigateTo(Page.CHECKUP);
    return flushTasks();
  });

  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(`Title shown correctly for ${type}`, async function() {
            Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, type);
            passwordManager.data.insecureCredentials = [
              makeInsecureCredential({
                types: [
                  CompromiseType.LEAKED,
                  CompromiseType.REUSED,
                  CompromiseType.WEAK,
                ],
              }),
              makeInsecureCredential({
                types: [
                  CompromiseType.PHISHED,
                  CompromiseType.REUSED,
                  CompromiseType.WEAK,
                ],
              }),
            ];

            const section = document.createElement('checkup-details-section');
            document.body.appendChild(section);
            await passwordManager.whenCalled('getInsecureCredentials');
            const params = await pluralString.whenCalled('getPluralString');
            await flushTasks();

            assertEquals(type + 'Passwords', params.messageName);
            assertEquals(2, params.itemCount);
          }));
});
