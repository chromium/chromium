// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';

import {PrivacySandboxCombinedDialogAppElement, PrivacySandboxCombinedDialogStep} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';
import {PrivacySandboxDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_app.js';
import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_browser_proxy.js';
import {PrivacySandboxNoticeDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_dialog_app.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

class TestPrivacySandboxDialogBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxDialogBrowserProxy {
  constructor() {
    super(['promptActionOccurred', 'resizeDialog', 'showDialog']);
  }

  promptActionOccurred() {
    this.methodCalled('promptActionOccurred', arguments);
  }

  resizeDialog() {
    this.methodCalled('resizeDialog', arguments);
    return Promise.resolve();
  }

  showDialog() {
    this.methodCalled('showDialog');
  }
}

suite('PrivacySandboxDialogConsent', function() {
  let page: PrivacySandboxDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(
      buttonSelector: string, element: HTMLElement = page) {
    const actionButton =
        element.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isConsent: true,
    });
  });

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-dialog-app');
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('dialogStructure', function() {
    // Consent dialog has addditionally an expand button and H2 title. It also
    // has a different set of buttons.
    assertTrue(isChildVisible(page, '.header h2'));
    assertTrue(isChildVisible(page, '.header h3'));

    assertTrue(isChildVisible(page, '.section'));

    assertTrue(isChildVisible(page, '#expandSection cr-expand-button'));

    assertTrue(isChildVisible(page, '#declineButton'));
    assertTrue(isChildVisible(page, '#confirmButton'));
    assertFalse(isChildVisible(page, '#settingsButton'));
    assertFalse(isChildVisible(page, '#ackButton'));
  });

  test('acceptClicked', async function() {
    testClickButton('#confirmButton');
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.CONSENT_ACCEPTED);
  });

  test('declineClicked', async function() {
    testClickButton('#declineButton');
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.CONSENT_DECLINED);
  });

  test('learnMoreClicked', async function() {
    // In the initial state, the content area isn't scrollable and doesn't have
    // a separator in the bottom (represented by 'can-scroll' class).
    // The collapse section is closed.
    const collapseElement = page.shadowRoot!.querySelector('iron-collapse');
    const contentArea: HTMLElement|null =
        page.shadowRoot!.querySelector('#contentArea');
    let hasScrollbar = contentArea!.offsetHeight < contentArea!.scrollHeight;
    assertFalse(collapseElement!.opened);
    assertEquals(contentArea!.classList.contains('can-scroll'), hasScrollbar);

    // After clicking on the collapse section, the content area expands and
    // becomes scrollable with a separator in the bottom. The collapse section
    // is opened and the native UI is notified about the action.
    testClickButton('#expandSection cr-expand-button');
    // TODO(crbug.com/1286276): Add testing for the scroll position.
    const [openedAction] =
        await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(
        openedAction, PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);
    assertTrue(contentArea!.classList.contains('can-scroll'));

    // Reset proxy in between button clicks.
    browserProxy.reset();

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('#expandSection cr-expand-button');
    const [closedAction] =
        await browserProxy.whenCalled('promptActionOccurred');
    hasScrollbar = contentArea!.offsetHeight < contentArea!.scrollHeight;
    assertEquals(
        closedAction, PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
    assertEquals(contentArea!.classList.contains('can-scroll'), hasScrollbar);
  });

  test('escPressed', async function() {
    browserProxy.reset();
    pressAndReleaseKeyOn(page, 0, '', 'Escape');
    // No user action is triggered by pressing Esc.
    assertEquals(browserProxy.getCallCount('promptActionOccurred'), 0);
  });
});

suite('PrivacySandboxDialogNotice', function() {
  let page: PrivacySandboxDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(buttonSelector: string) {
    const actionButton =
        page.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isConsent: false,
    });
  });

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-dialog-app');
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('dialogStructure', function() {
    // Notice dialog doesn't have an expand button and H2 title. It also has
    // a different set of buttons.
    assertFalse(isChildVisible(page, '.header h2'));
    assertTrue(isChildVisible(page, '.header h3'));

    assertTrue(isChildVisible(page, '.section'));

    assertFalse(isChildVisible(page, '#expandSection cr-expand-button'));

    assertFalse(isChildVisible(page, '#declineButton'));
    assertFalse(isChildVisible(page, '#confirmButton'));
    assertTrue(isChildVisible(page, '#settingsButton'));
    assertTrue(isChildVisible(page, '#ackButton'));
  });

  test('ackClicked', async function() {
    testClickButton('#ackButton');
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    testClickButton('#settingsButton');
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('escPressed', async function() {
    pressAndReleaseKeyOn(page, 0, '', 'Escape');
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.NOTICE_DISMISS);
  });
});

suite('PrivacySandboxDialogCombined', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(
      buttonSelector: string, element: HTMLElement|null = page) {
    const actionButton =
        element!.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  async function verifyActionOccured(targetAction: PrivacySandboxPromptAction) {
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, targetAction);
    browserProxy.reset();
  }

  function getActiveStep(): HTMLElement|null {
    return page.shadowRoot!.querySelector('.active');
  }

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-combined-dialog-app');
    page.disableAnimationsForTesting();
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('acceptConsentAndAckNotice', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep: HTMLElement|null = getActiveStep();
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Accept the consent step.
    testClickButton('#confirmButton', consentStep);
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_ACCEPTED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('acceptConsentAndOpenSettings', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep: HTMLElement|null = getActiveStep();
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Accept the consent step.
    testClickButton('#confirmButton', consentStep);
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_ACCEPTED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Click 'Open settings' button.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('declineConsentAndAckNotice', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep: HTMLElement|null = getActiveStep();
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Decline the consent step.
    testClickButton('#declineButton', consentStep);
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_DECLINED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('declineConsentAndOpenSettings', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep: HTMLElement|null = getActiveStep();
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Decline the consent step.
    testClickButton('#declineButton', consentStep);
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_DECLINED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Click 'Open settings' button.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep: HTMLElement|null = getActiveStep();
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);
    // TODO(crbug.com/1378703): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement = consentStep!.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    const collapseElement =
        learnMoreElement!.shadowRoot!.querySelector('iron-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
  });
});

suite('PrivacySandboxDialogNoticeEEA', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(
      buttonSelector: string, element: HTMLElement|null = page) {
    const actionButton =
        element!.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  async function verifyActionOccured(targetAction: PrivacySandboxPromptAction) {
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, targetAction);
    browserProxy.reset();
  }

  function getActiveStep(): HTMLElement|null {
    return page.shadowRoot!.querySelector('.active');
  }

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    window.history.replaceState({}, '', '?step=notice');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-combined-dialog-app');
    page.disableAnimationsForTesting();
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('ackClicked', async function() {
    // Verify that dialog starts with notice step.
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    // Verify that dialog starts with notice step.
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep: HTMLElement|null = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);
    // TODO(crbug.com/1378703): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement = noticeStep!.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    const collapseElement =
        learnMoreElement!.shadowRoot!.querySelector('iron-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        PrivacySandboxPromptAction.NOTICE_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        PrivacySandboxPromptAction.NOTICE_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
  });
});

suite('PrivacySandboxDialogNoticeROW', function() {
  let page: PrivacySandboxNoticeDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function testClickButton(
      buttonSelector: string, element: HTMLElement|null = page) {
    const actionButton =
        element!.shadowRoot!.querySelector(buttonSelector) as CrButtonElement;
    actionButton.click();
  }

  async function verifyActionOccured(targetAction: PrivacySandboxPromptAction) {
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, targetAction);
    browserProxy.reset();
  }

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-notice-dialog-app');
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('ackClicked', async function() {
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    testClickButton('#ackButton');
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    testClickButton('#settingsButton');
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(PrivacySandboxPromptAction.NOTICE_SHOWN);
    // TODO(crbug.com/1378703): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement =
        page.shadowRoot!.querySelector('privacy-sandbox-dialog-learn-more');
    const collapseElement =
        learnMoreElement!.shadowRoot!.querySelector('iron-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        PrivacySandboxPromptAction.NOTICE_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        PrivacySandboxPromptAction.NOTICE_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
  });
});
