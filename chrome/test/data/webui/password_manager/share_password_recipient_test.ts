// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {makeRecipientInfo} from './test_util.js';

suite('SharePasswordRecipientTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('Has correct eliglible state', function() {
    const recipient = makeRecipientInfo(/*isEligible=*/true);
    const element = document.createElement('share-password-recipient');
    element.recipient = recipient;
    document.body.appendChild(element);
    flush();

    assertTrue(isVisible(element.$.avatar));
    assertTrue(isVisible(element.$.name));
    assertTrue(isVisible(element.$.email));

    assertEquals(recipient.profileImageUrl, element.$.avatar.src);
    assertEquals(recipient.displayName, element.$.name.textContent);
    assertEquals(recipient.email, element.$.email.textContent);
  });
});
