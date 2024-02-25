// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {SharePasswordRecipientElement} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {makeRecipientInfo} from './test_util.js';

suite('SharePasswordRecipientTest', function() {
  let element: SharePasswordRecipientElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('share-password-recipient');
    document.body.appendChild(element);
    flush();
  });

  test('Has correct eliglible state', function() {
    const recipient = makeRecipientInfo(/*isEligible=*/ true);
    element.recipient = recipient;
    flush();

    assertTrue(isVisible(element.$.avatar));
    assertTrue(isVisible(element.$.name));
    assertTrue(isVisible(element.$.email));

    assertEquals(recipient.profileImageUrl, element.$.avatar.src);
    assertEquals(recipient.displayName, element.$.name.textContent);
    assertEquals(recipient.email, element.$.email.textContent);

    const tooltip = element.shadowRoot!.querySelector('cr-tooltip-icon');
    assertFalse(!!tooltip);
    const notAvailable = element.shadowRoot!.querySelector('#notAvailable');
    assertFalse(!!notAvailable);
  });

  test('Has correct ineliglible/disabled state', function() {
    const recipient = makeRecipientInfo(/*isEligible=*/ false);
    element.recipient = recipient;
    element.disabled = true;
    flush();

    assertTrue(isVisible(element.$.avatar));
    assertTrue(isVisible(element.$.name));
    assertTrue(isVisible(element.$.email));

    const tooltip = element.shadowRoot!.querySelector('cr-tooltip-icon');
    assertTrue(!!tooltip);
    assertTrue(isVisible(tooltip));
    assertEquals(
        tooltip.tooltipText,
        loadTimeData.getString('sharePasswordMemeberUnavailable'));

    const notAvailable = element.shadowRoot!.querySelector('#notAvailable');
    assertTrue(!!notAvailable);
    assertTrue(isVisible(notAvailable));
    assertEquals(
        notAvailable.textContent,
        loadTimeData.getString('sharePasswordNotAvailable'));
  });

  test('Non-disabled element is selectable', async function() {
    const recipient = makeRecipientInfo(/*isEligible=*/ true);
    element.recipient = recipient;
    flush();

    assertFalse(element.selected);
    assertFalse(element.$.checkbox.checked);
    element.click();
    await element.$.checkbox.updateComplete;

    assertTrue(element.selected);
    assertTrue(element.$.checkbox.checked);
  });

  test('Disabled element is not selectable', async function() {
    const recipient = makeRecipientInfo(/*isEligible=*/ false);
    element.recipient = recipient;
    element.disabled = true;
    flush();

    assertFalse(element.selected);
    assertFalse(element.$.checkbox.checked);
    element.click();
    await element.$.checkbox.updateComplete;

    assertFalse(element.selected);
    assertFalse(element.$.checkbox.checked);
  });
});
