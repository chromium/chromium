// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SettingsAiInfoCardElement} from 'chrome://settings/settings.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('AiInfoCard', function() {
  let infoCard: SettingsAiInfoCardElement;

  function createInfoCard() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    infoCard = document.createElement('settings-ai-info-card');
    document.body.appendChild(infoCard);
  }

  test('LoggingInfo', () => {
    createInfoCard();
    const thirdRow =
        infoCard.shadowRoot.querySelector<HTMLElement>('li:nth-child(3)');
    assertTrue(!!thirdRow);
    assertEquals(
        thirdRow.innerText, loadTimeData.getString('aiPageMainSublabel3'));
    // The non-managed string is not supposed to contain a learn more link.
    assertFalse(!!thirdRow.querySelector('a'));
    const crIcon = thirdRow.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(crIcon.icon, 'settings20:account-box');
  });

  test('LoggingInfoManaged', () => {
    loadTimeData.overrideValues({'isManaged': true});
    createInfoCard();
    const thirdRow =
        infoCard.shadowRoot.querySelector<HTMLElement>('li:nth-child(3)');
    assertTrue(!!thirdRow);
    assertStringContains(thirdRow.innerText, 'Your organization manages how');
    // The managed string contains a learn more link.
    assertTrue(!!thirdRow.querySelector('a'));
    const crIcon = thirdRow.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(crIcon.icon, loadTimeData.getString('managedByIcon'));
  });
});
