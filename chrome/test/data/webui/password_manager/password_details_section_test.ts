// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PasswordDetailsSectionElement, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup} from './test_util.js';

suite('PasswordDetailsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    return flushTasks();
  });

  test('Navigation from passwords section', async function() {
    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    // Simulate navigation from passwords list.
    const group = createCredentialGroup({name: 'test.com'});
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const title = section.shadowRoot!.querySelector('#title');
    assertTrue(!!title);
    assertEquals(group.name, title.textContent!.trim());
  });

  test('Navigating directly', async function() {
    // Simulate direct navigation.
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'test.com');
    passwordManager.data.groups = [
      createCredentialGroup({name: 'test.com'}),
      createCredentialGroup({name: 'test1.com'}),
      createCredentialGroup({name: 'test2.com'}),
    ];

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    const title = section.$.title;
    assertTrue(!!title);
    assertEquals('test.com', title.textContent!.trim());
  });

  test('Navigating directly fails when group is not found', async function() {
    // Simulate direct navigation.
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'test.com');
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('Clicking back navigates to passwords section', async function() {
    const group = createCredentialGroup({name: 'test.com'});
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    const section: PasswordDetailsSectionElement =
        document.createElement('password-details-section');
    document.body.appendChild(section);
    await flushTasks();

    const backButton = section.$.backButton;

    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    backButton.click();
    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });
});
