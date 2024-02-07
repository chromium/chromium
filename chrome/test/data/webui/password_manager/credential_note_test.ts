// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CredentialNoteElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

async function createNoteElement(note: string): Promise<CredentialNoteElement> {
  const element = document.createElement('credential-note');
  // Fix the width so we can test hiding a long note.
  element.style.display = 'block';
  element.style.width = '100px';
  element.note = note;
  document.body.appendChild(element);
  await flushTasks();
  return element;
}

suite('CredentialNoteTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    return flushTasks();
  });

  test('empty note shows correct string', async function() {
    const element = await createNoteElement('');

    assertEquals(
        loadTimeData.getString('emptyNote'),
        element.$.noteValue.textContent!.trim());
    assertTrue(element.$.showMore.hidden);
  });

  test('short note is shown fully', async function() {
    const note = 'Remember the milk';
    const element = await createNoteElement(note);

    assertEquals(note, element.$.noteValue.textContent!.trim());
    assertTrue(element.$.showMore.hidden);
  });

  test('long note is shown fully', async function() {
    const note =
        'It is a long established fact that a reader will be distracted by ' +
        'the readable content of a page when looking at its layout. The ' +
        'point of using Lorem Ipsum is that it has a more-or-less normal ' +
        'distribution of letters, as opposed to using \'Content here, ' +
        'content here\', making it look like readable English.';

    const element = await createNoteElement(note);

    assertEquals(note, element.$.noteValue.textContent!.trim());
    assertTrue(element.$.noteValue.hasAttribute('limit-note'));
    assertFalse(element.$.showMore.hidden);

    // Open note fully
    element.$.showMore.click();
    assertFalse(element.$.noteValue.hasAttribute('limit-note'));
    await passwordManager.whenCalled('extendAuthValidity');
  });
});
