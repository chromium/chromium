// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://user-education-internals/user_education_internals.js';

import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
import type {UserEducationInternalsElement} from 'chrome://user-education-internals/user_education_internals.js';
import {browserProxyFactory} from 'chrome://user-education-internals/user_education_internals.mojom-webui.js';
import type {UserEducationWhatsNewInternalsCardElement} from 'chrome://user-education-internals/user_education_whats_new_internals_card.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestUserEducationInternalsPageHandler} from './test_user_education_internals_page_handler.js';

suite('UserEducationInternalsTest', function() {
  let element: UserEducationInternalsElement;
  let handler: TestUserEducationInternalsPageHandler;

  async function expandAndGetClearButton(cardSelector: string): Promise<{
    card: UserEducationWhatsNewInternalsCardElement,
    clearButton: HTMLElement,
  }> {
    const card = element.shadowRoot
                     .querySelector<UserEducationWhatsNewInternalsCardElement>(
                         cardSelector);
    assertTrue(!!card);

    const expandButton = card.shadowRoot.querySelector('cr-expand-button');
    assertTrue(!!expandButton);
    if (!expandButton.expanded) {
      expandButton.click();
      await microtasksFinished();
    }

    const clearButton = card.shadowRoot.querySelector<HTMLElement>('#clear');
    assertTrue(!!clearButton);
    assertTrue(isVisible(clearButton));
    return {card, clearButton};
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    TrackedElementManager.getInstance().reset();

    handler = new TestUserEducationInternalsPageHandler();
    browserProxyFactory.setInstance({handler});

    element = document.createElement('user-education-internals');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  // <if expr="not is_chromeos">
  test('ClearWhatsNewData', async function() {
    const clearButton =
        element.shadowRoot.querySelector<HTMLElement>('#clearWhatsNewData');
    assertTrue(!!clearButton);
    clearButton.click();
    await handler.whenCalled('clearWhatsNewData');
    await microtasksFinished();

    // After clearing all, editions and modules have both been re-fetched.
    assertEquals(1, handler.getCallCount('clearWhatsNewData'));
    assertEquals(2, handler.getCallCount('getWhatsNewModules'));
    assertEquals(2, handler.getCallCount('getWhatsNewEditions'));
  });
  // </if>

  test('ClearWhatsNewModuleDataFromCard', async function() {
    const {card, clearButton} = await expandAndGetClearButton('#TestModule');

    // Set response for when modules are re-fetched after clearing.
    handler.setWhatsNewModules([{
      displayTitle: 'Test Module',
      moduleName: 'TestModule',
      hasBrowserCommand: true,
      isFeatureEnabled: true,
      queuePosition: -1,
    }]);
    clearButton.click();
    const moduleName = await handler.whenCalled('clearWhatsNewModuleData');
    await microtasksFinished();

    assertEquals('TestModule', moduleName);
    assertEquals(1, handler.getCallCount('clearWhatsNewModuleData'));
    assertEquals(0, handler.getCallCount('clearWhatsNewData'));
    // Modules have been re-fetched.
    assertEquals(2, handler.getCallCount('getWhatsNewModules'));
    assertEquals(1, handler.getCallCount('getWhatsNewEditions'));
    assertFalse(element.$.errorMessageToast.open);
    assertTrue(card.shadowRoot.querySelector('#data')!.textContent.includes(
        'Not in queue'));
  });

  test('ClearWhatsNewEditionDataFromCard', async function() {
    const {card, clearButton} = await expandAndGetClearButton('#TestEdition');

    // Set response for when editions are re-fetched after clearing.
    handler.setWhatsNewEditions([{
      displayTitle: 'Test Edition',
      editionName: 'TestEdition',
      isFeatureEnabled: true,
      hasBeenUsed: false,
      versionUsed: 0,
    }]);
    clearButton.click();
    const editionName = await handler.whenCalled('clearWhatsNewEditionData');
    await microtasksFinished();

    assertEquals('TestEdition', editionName);
    assertEquals(1, handler.getCallCount('clearWhatsNewEditionData'));
    assertEquals(0, handler.getCallCount('clearWhatsNewData'));
    assertEquals(1, handler.getCallCount('getWhatsNewModules'));
    // Editions have been re-fetched.
    assertEquals(2, handler.getCallCount('getWhatsNewEditions'));
    assertFalse(element.$.errorMessageToast.open);
    assertTrue(card.shadowRoot.querySelector('#data')!.textContent.includes(
        'Has been used: no'));
  });

  test('ClearWhatsNewDataErrorShowsToast', async function() {
    handler.setClearWhatsNewErrorMessage('Cannot get registry');

    const {clearButton} = await expandAndGetClearButton('#TestModule');
    clearButton.click();
    await handler.whenCalled('clearWhatsNewModuleData');
    await microtasksFinished();

    assertTrue(element.$.errorMessageToast.open);
    assertEquals(
        'Cannot get registry',
        element.shadowRoot.querySelector('#errorMessage')!.textContent.trim());
    // Nothing was cleared due to error, so no re-fetches occurred.
    assertEquals(1, handler.getCallCount('getWhatsNewModules'));
    assertEquals(1, handler.getCallCount('getWhatsNewEditions'));
  });
});
