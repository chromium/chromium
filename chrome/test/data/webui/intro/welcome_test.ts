// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/welcome/app.js';

import {browserProxyFactory as welcomeMojoProxyFactory, WelcomePageHandlerRemote} from 'chrome://intro/welcome.mojom-webui.js';
import type {WelcomeAppElement} from 'chrome://intro/welcome/app.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('WelcomeTest', function() {
  let testElement: WelcomeAppElement;
  let handler: TestMock<WelcomePageHandlerRemote>&WelcomePageHandlerRemote;

  async function createTestElement(): Promise<WelcomeAppElement> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('welcome-app');
    document.body.appendChild(testElement);
    await microtasksFinished();
    return testElement;
  }

  setup(async function() {
    loadTimeData.overrideValues({showDefaultBrowserToggle: true});

    handler = TestMock.fromClass(WelcomePageHandlerRemote);
    welcomeMojoProxyFactory.setInstance({handler});

    await createTestElement();
  });

  test('ElementsExist', function() {
    const acceptButton = testElement.$.acceptButton;
    assertTrue(acceptButton.classList.contains('action-button'));
    assertFalse(acceptButton.disabled);

    const toggle =
        testElement.shadowRoot.querySelector<CrToggleElement>(
            '#default-browser-toggle')!;
    assertTrue(isVisible(toggle));
    assertFalse(toggle.disabled);
    assertTrue(toggle.checked);
    assertEquals(
        'default-browser-label', toggle.getAttribute('aria-labelledby'));

    const label =
        testElement.shadowRoot.querySelector('#default-browser-label');
    assertTrue(isVisible(label));
  });

  test('AcceptButtonClickDisablesToggle', async function() {
    const acceptButton = testElement.$.acceptButton;
    const toggle =
        testElement.shadowRoot.querySelector<CrToggleElement>(
            '#default-browser-toggle')!;

    acceptButton.click();
    await handler.whenCalled('continue');
    await microtasksFinished();

    assertTrue(acceptButton.disabled);
    assertTrue(toggle.disabled);
  });

  test('AcceptButtonClicked', async function() {
    const acceptButton = testElement.$.acceptButton;
    assertFalse(acceptButton.disabled);

    acceptButton.click();
    const [isUmaOptIn, isDefaultBrowser] =
        await handler.whenCalled('continue');
    assertEquals(null, isUmaOptIn);
    assertTrue(isDefaultBrowser);

    await microtasksFinished();
    assertTrue(acceptButton.disabled);
  });

  test('AcceptButtonClickedWithDefaultBrowserDisabled', async function() {
    const toggle =
        testElement.shadowRoot.querySelector<CrToggleElement>(
            '#default-browser-toggle')!;
    toggle.click();
    await microtasksFinished();
    assertFalse(toggle.checked);

    const acceptButton = testElement.$.acceptButton;
    acceptButton.click();
    const [isUmaOptIn, isDefaultBrowser] =
        await handler.whenCalled('continue');
    assertEquals(null, isUmaOptIn);
    assertFalse(isDefaultBrowser);

    await microtasksFinished();
    assertTrue(acceptButton.disabled);
  });

  test('DefaultBrowserToggleHiddenWhenNotSupported', async function() {
    loadTimeData.overrideValues({showDefaultBrowserToggle: false});
    await createTestElement();

    const toggle =
        testElement.shadowRoot.querySelector<CrToggleElement>(
            '#default-browser-toggle');
    assertFalse(isVisible(toggle));
    const container =
        testElement.shadowRoot.querySelector('#default-browser-container');
    assertFalse(isVisible(container));

    const acceptButton = testElement.$.acceptButton;
    assertTrue(isVisible(acceptButton));
    acceptButton.click();
    const [isUmaOptIn, isDefaultBrowser] =
        await handler.whenCalled('continue');
    assertEquals(null, isUmaOptIn);
    assertEquals(null, isDefaultBrowser);

    await microtasksFinished();
    assertTrue(acceptButton.disabled);
  });
});

