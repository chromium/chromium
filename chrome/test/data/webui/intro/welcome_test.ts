// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/welcome/app.js';

import {browserProxyFactory as welcomeMojoProxyFactory, WelcomePageHandlerRemote} from 'chrome://intro/welcome.mojom-webui.js';
import type {WelcomeAppElement} from 'chrome://intro/welcome/app.js';
import type {LocalizedLinkElement} from 'chrome://resources/cr_components/localized_link/localized_link.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
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
    loadTimeData.overrideValues({
      showDefaultBrowserToggle: true,
      showMetricsOptIn: true,
      welcomeMetricsLabel:
          'You’re helping make Chrome better by sending usage statistics and crash reports to Google. <a href="#">Manage</a>',
      welcomeMetricsOffLabel:
          'You’re not sending usage statistics and crash reports to Google. <a href="#">Manage</a>',
      welcomeMetricsPopupTurnOffButtonLabel: 'Turn off',
      welcomeMetricsPopupTurnOnButtonLabel: 'Turn on',
    });

    handler = TestMock.fromClass(WelcomePageHandlerRemote);
    welcomeMojoProxyFactory.setInstance({handler});

    await createTestElement();
  });

  function getToast(): CrToastElement {
    const toast =
        testElement.shadowRoot.querySelector<CrToastElement>('#toast');
    assertTrue(!!toast);
    return toast;
  }

  function getToastActionButton(): CrButtonElement {
    const actionButton = testElement.shadowRoot.querySelector<CrButtonElement>(
        '#toastActionButton');
    assertTrue(!!actionButton);
    return actionButton;
  }

  function getToastCloseButton(): CrIconButtonElement {
    const closeButton =
        testElement.shadowRoot.querySelector<CrIconButtonElement>(
            '#toastCloseButton');
    assertTrue(!!closeButton);
    return closeButton;
  }

  function getManageMetricsLink(): HTMLAnchorElement {
    const localizedLink =
        testElement.shadowRoot.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const manageLink =
        localizedLink.shadowRoot.querySelector<HTMLAnchorElement>('a');
    assertTrue(!!manageLink);
    return manageLink;
  }

  function getFooterLocalizedString(): string {
    const localizedLink =
        testElement.shadowRoot.querySelector<LocalizedLinkElement>(
            'localized-link');
    assertTrue(!!localizedLink);
    return `${localizedLink.localizedString}`;
  }

  function verifyContinueArgs(
      expectedIsUmaOptIn: boolean|null,
      expectedIsDefaultBrowser: boolean|null) {
    assertEquals(1, handler.getCallCount('continue'));
    const [isUmaOptIn, isDefaultBrowser] = handler.getArgs('continue')[0];
    assertEquals(expectedIsUmaOptIn, isUmaOptIn);
    assertEquals(expectedIsDefaultBrowser, isDefaultBrowser);
  }

  test('ElementsExist', function() {
    const acceptButton = testElement.$.acceptButton;
    assertTrue(acceptButton.classList.contains('action-button'));
    assertFalse(acceptButton.disabled);

    assertTrue(!!testElement.shadowRoot.querySelector('#toast'));
    assertFalse(getToast().open);

    const toggle =
        testElement.shadowRoot.querySelector<CrToggleElement>(
            '#default-browser-toggle')!;
    assertTrue(isVisible(toggle));
    assertFalse(toggle.disabled);
    assertTrue(toggle.checked);
    assertEquals(
        'default-browser-label', toggle.getAttribute('aria-labelledby'));

    assertTrue(isVisible(
        testElement.shadowRoot.querySelector('#default-browser-label')));
    assertTrue(
        isVisible(testElement.shadowRoot.querySelector('localized-link')));
  });

  test('AcceptButtonClicked', async function() {
    const acceptButton = testElement.$.acceptButton;
    const toggle = testElement.shadowRoot.querySelector<CrToggleElement>(
        '#default-browser-toggle')!;
    const localizedLink =
        testElement.shadowRoot.querySelector<LocalizedLinkElement>(
            'localized-link')!;
    assertFalse(acceptButton.disabled);
    assertFalse(toggle.disabled);
    assertFalse(localizedLink.linkDisabled);

    getManageMetricsLink().click();
    await microtasksFinished();
    assertTrue(getToast().open);

    acceptButton.click();
    await microtasksFinished();

    verifyContinueArgs(
        /*expectedIsUmaOptIn=*/ true, /*expectedIsDefaultBrowser=*/ true);
    assertTrue(acceptButton.disabled);
    assertTrue(toggle.disabled);
    assertTrue(localizedLink.linkDisabled);
    assertFalse(getToast().open);

    // Clicking the link after accept should be a no-op and not open the toast.
    getManageMetricsLink().click();
    await microtasksFinished();
    assertFalse(getToast().open);
  });

  test('FooterManageClicked', async function() {
    const toast = getToast();
    assertFalse(toast.open);

    getManageMetricsLink().click();
    await microtasksFinished();

    assertTrue(toast.open);
  });

  test('ToastCloseButtonClicked', async function() {
    const toast = getToast();
    toast.show();
    getToastCloseButton().click();
    await microtasksFinished();

    assertFalse(toast.open);
  });

  test('ToastActionButtonClosesToast', async function() {
    const toast = getToast();
    getManageMetricsLink().click();
    await microtasksFinished();
    assertTrue(toast.open);

    getToastActionButton().click();
    await microtasksFinished();
    assertFalse(toast.open);
  });

  test('LabelsUpdateWhenMetricsToggled', async function() {
    assertEquals(
        testElement.i18nAdvanced('welcomeMetricsLabel').toString(),
        getFooterLocalizedString());

    getManageMetricsLink().click();
    await microtasksFinished();

    const actionButton = getToastActionButton();
    assertEquals(
        testElement.i18n('welcomeMetricsPopupTurnOffButtonLabel'),
        actionButton.textContent.trim());
    actionButton.click();
    await microtasksFinished();

    // Labels should be updated to the "off" state.
    assertEquals(
        testElement.i18nAdvanced('welcomeMetricsOffLabel').toString(),
        getFooterLocalizedString());

    getManageMetricsLink().click();
    await microtasksFinished();
    assertEquals(
        testElement.i18n('welcomeMetricsPopupTurnOnButtonLabel'),
        actionButton.textContent.trim());

    actionButton.click();
    await microtasksFinished();

    // Labels should be updated to the "on" state.
    assertEquals(
        testElement.i18nAdvanced('welcomeMetricsLabel').toString(),
        getFooterLocalizedString());
    getManageMetricsLink().click();
    await microtasksFinished();
    assertEquals(
        testElement.i18n('welcomeMetricsPopupTurnOffButtonLabel'),
        actionButton.textContent.trim());
  });

  test('AcceptButtonClickedWithMetricsDisabled', async function() {
    getManageMetricsLink().click();
    await microtasksFinished();
    getToastActionButton().click();
    await microtasksFinished();

    const acceptButton = testElement.$.acceptButton;
    acceptButton.click();
    await microtasksFinished();

    verifyContinueArgs(
        /*expectedIsUmaOptIn=*/ false, /*expectedIsDefaultBrowser=*/ true);
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
    await microtasksFinished();

    verifyContinueArgs(
        /*expectedIsUmaOptIn=*/ true, /*expectedIsDefaultBrowser=*/ false);
  });

  test('DefaultBrowserToggleHiddenWhenNotSupported', async function() {
    loadTimeData.overrideValues({showDefaultBrowserToggle: false});
    await createTestElement();

    assertFalse(isVisible(
        testElement.shadowRoot.querySelector('#default-browser-toggle')));
    assertFalse(isVisible(
        testElement.shadowRoot.querySelector('#default-browser-container')));

    const acceptButton = testElement.$.acceptButton;
    assertTrue(isVisible(acceptButton));
    acceptButton.click();
    await microtasksFinished();

    verifyContinueArgs(
        /*expectedIsUmaOptIn=*/ true, /*expectedIsDefaultBrowser=*/ null);
  });

  test('MetricsOptInHiddenWhenNotSupported', async function() {
    loadTimeData.overrideValues({showMetricsOptIn: false});
    await createTestElement();

    assertFalse(isVisible(testElement.shadowRoot.querySelector('#footer')));
    assertFalse(isVisible(testElement.shadowRoot.querySelector('#toast')));

    const acceptButton = testElement.$.acceptButton;
    assertTrue(isVisible(acceptButton));
    acceptButton.click();
    await microtasksFinished();

    verifyContinueArgs(
        /*expectedIsUmaOptIn=*/ null, /*expectedIsDefaultBrowser=*/ true);
  });
});
