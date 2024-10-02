// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_restricted_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';

import type {PrivacySandboxCombinedDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';
import {PrivacySandboxCombinedDialogStep} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';
import type {PrivacySandboxDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_app.js';
import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_browser_proxy.js';
import type {PrivacySandboxDialogConsentStepElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_consent_step.js';
import {PrivacySandboxDialogMixin} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_mixin.js';
import type {PrivacySandboxDialogNoticeStepElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_notice_step.js';
import type {PrivacySandboxNoticeDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_dialog_app.js';
import type {PrivacySandboxNoticeRestrictedDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_restricted_dialog_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

class TestPrivacySandboxDialogBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxDialogBrowserProxy {
  private privacySandboxShouldShowPrivacyPolicy_ = false;
  constructor() {
    super([
      'promptActionOccurred',
      'resizeDialog',
      'showDialog',
      'recordPrivacyPolicyLoadTime',
      'shouldShowPrivacySandboxPrivacyPolicy',
    ]);
  }

  setPrivacySandboxShouldShowPrivacyPolicy(shouldShow: boolean) {
    this.privacySandboxShouldShowPrivacyPolicy_ = shouldShow;
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

  recordPrivacyPolicyLoadTime() {
    this.methodCalled('recordPrivacyPolicyLoadTime', arguments);
  }

  shouldShowPrivacySandboxPrivacyPolicy() {
    this.methodCalled('shouldShowPrivacySandboxPrivacyPolicy');
    return Promise.resolve(this.privacySandboxShouldShowPrivacyPolicy_);
  }
}

function isChildInParentBounds(
    viewport: HTMLElement, targetSelector: string): boolean {
  const target = viewport.shadowRoot!.querySelector(targetSelector);
  assertTrue(
      !!target, `target element ${targetSelector} not found in the viewport`);
  const targetBounds = target!.getBoundingClientRect();
  const viewportBounds = viewport.getBoundingClientRect();
  return targetBounds.top >= 0 && targetBounds.left >= 0 &&
      targetBounds.top < viewportBounds.height &&
      targetBounds.bottom <= viewportBounds.height &&
      targetBounds.left < viewportBounds.width &&
      targetBounds.right <= viewportBounds.width;
}

// Wait until there hasn't been a scroll event for 100ms. It is sufficient
// for tests but shouldn't be used in production.
async function waitForScrollToFinish(scrollable: HTMLElement) {
  await new Promise(resolve => {
    let timeout: number;
    scrollable.addEventListener('scroll', () => {
      clearTimeout(timeout);
      timeout = setTimeout(resolve, 100);
    });
  });
}

function doesElemenHaveScrollbar(element: HTMLElement) {
  return element.clientHeight < element.scrollHeight;
}

function isAllContentVisible(element: HTMLElement) {
  const lastBottomMargin = 8;
  return element.clientHeight >= element.scrollHeight - lastBottomMargin;
}

async function verifyActionOccured(
    browserProxy: TestPrivacySandboxDialogBrowserProxy,
    targetAction: PrivacySandboxPromptAction) {
  const [action] = await browserProxy.whenCalled('promptActionOccurred');
  assertEquals(action, targetAction);
  browserProxy.reset();
}

function testClickButton(buttonSelector: string, element: HTMLElement|null) {
  const actionButton =
      element!.shadowRoot!.querySelector<HTMLElement>(buttonSelector);
  assertTrue(
      !!actionButton, `the button isn\'t found, selector: ${buttonSelector}`);
  actionButton.click();
}

suite('Consent', function() {
  let page: PrivacySandboxDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

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
    testClickButton('#confirmButton', page);
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.CONSENT_ACCEPTED);
  });

  test('declineClicked', async function() {
    testClickButton('#declineButton', page);
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.CONSENT_DECLINED);
  });

  test('learnMoreClicked', async function() {
    // In the initial state, the content area isn't scrollable and doesn't have
    // a separator in the bottom (represented by 'can-scroll' class).
    // The collapse section is closed.
    const collapseElement = page.shadowRoot!.querySelector('cr-collapse');
    const contentArea: HTMLElement|null =
        page.shadowRoot!.querySelector('#contentArea');
    let hasScrollbar = doesElemenHaveScrollbar(contentArea!);
    assertFalse(collapseElement!.opened);
    assertEquals(contentArea!.classList.contains('can-scroll'), hasScrollbar);

    // After clicking on the collapse section, the content area expands and
    // becomes scrollable with a separator in the bottom. The collapse section
    // is opened and the native UI is notified about the action.
    testClickButton('#expandSection cr-expand-button', page);
    // TODO(crbug.com/40210776): Add testing for the scroll position.
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
    testClickButton('#expandSection cr-expand-button', page);
    const [closedAction] =
        await browserProxy.whenCalled('promptActionOccurred');
    hasScrollbar = doesElemenHaveScrollbar(contentArea!);
    assertEquals(
        closedAction, PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
    assertEquals(contentArea!.classList.contains('can-scroll'), hasScrollbar);
  });

  test('escPressed', async function() {
    browserProxy.reset();
    pressAndReleaseKeyOn(page, 0, [], 'Escape');
    // No user action is triggered by pressing Esc.
    assertEquals(browserProxy.getCallCount('promptActionOccurred'), 0);
  });
});

suite('Notice', function() {
  let page: PrivacySandboxDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

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
    testClickButton('#ackButton', page);
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    testClickButton('#settingsButton', page);
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('escPressed', async function() {
    pressAndReleaseKeyOn(page, 0, [], 'Escape');
    const [action] = await browserProxy.whenCalled('promptActionOccurred');
    assertEquals(action, PrivacySandboxPromptAction.NOTICE_DISMISS);
  });
});

suite('Combined', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function getActiveStep(): PrivacySandboxDialogConsentStepElement|
      PrivacySandboxDialogNoticeStepElement {
    return page.shadowRoot!.querySelector('.active')!;
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

  test('moreButton', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.CONSENT);
    await flushTasks();

    const scrollable: HTMLElement =
        consentStep.shadowRoot!.querySelector('[scrollable]')!;
    // Turn-off scroll animations.
    scrollable.style.scrollBehavior = 'auto';
    const allContentVisible = isAllContentVisible(scrollable);

    assertEquals(
        isChildVisible(consentStep, '#moreButton'), !allContentVisible,
        `more button should only be visible when some of the dialog content \
        wasn't visible`);

    assertEquals(
        isChildVisible(consentStep, '#confirmButton'), true,
        `confirm button should never be hidden`);
    assertEquals(
        isChildInParentBounds(consentStep, '#confirmButton'), allContentVisible,
        allContentVisible ?
            'confirm button should visible if all content dialog is visible' :
            `confirm button should not be visible if some of the dialog
            content isn't visible from the start`);

    assertEquals(
        isChildVisible(consentStep, '#declineButton'), true,
        `decline button should never be hidden`);
    assertEquals(
        isChildInParentBounds(consentStep, '#declineButton'), allContentVisible,
        allContentVisible ?
            'decline button should visible if all content dialog is visible' :
            `decline button should not be visible if some of the dialog
            content isn't visible from the start`);

    if (allContentVisible) {
      return;
    }
    const moreButton: HTMLElement =
        consentStep.shadowRoot!.querySelector('#moreButton')!;
    moreButton.click();
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_BUTTON_CLICKED);
    await consentStep.whenWasScrolledToBottomForTest();

    // After scrolling down, the "More" button is hidden and dialog button are
    // visible in the parent bounds.
    assertEquals(
        isChildVisible(consentStep, '#moreButton'), false,
        'more button should not be visible anymore');
    assertEquals(
        isChildInParentBounds(consentStep, '#confirmButton'), true,
        'confirm button should be visible after scrolling to the bottom');
    assertEquals(
        isChildInParentBounds(consentStep, '#declineButton'), true,
        'decline button should be visible after scrolling to the bottom');
  });

  test('acceptConsentAndAckNotice', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Accept the consent step.
    testClickButton('#confirmButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_ACCEPTED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep()!;
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('acceptConsentAndOpenSettings', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Accept the consent step.
    testClickButton('#confirmButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_ACCEPTED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep()!;
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Click 'Open settings' button.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('declineConsentAndAckNotice', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Decline the consent step.
    testClickButton('#declineButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_DECLINED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep()!;
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('declineConsentAndOpenSettings', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Decline the consent step.
    testClickButton('#declineButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_DECLINED);

    // Resolving consent step triggers saving step.
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep()!;
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Click 'Open settings' button.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(consentStep!.id, PrivacySandboxCombinedDialogStep.CONSENT);
    // TODO(crbug.com/40244046): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement = consentStep!.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    const collapseElement =
        learnMoreElement!.shadowRoot!.querySelector('cr-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
  });

  test('privacyPolicyNotShown', async function() {
    browserProxy.setPrivacySandboxShouldShowPrivacyPolicy(false);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // The collapse section is opened.
    const learnMore: HTMLElement = consentStep!.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more')!;
    const collapseElement = learnMore!.shadowRoot!.querySelector('cr-collapse');
    testClickButton('cr-expand-button', learnMore);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    const learnMoreDiv = learnMore!.querySelector<HTMLElement>('#learnMoreDiv');
    assertTrue(!!learnMoreDiv);

    // Privacy policy div does not exist.
    assertEquals(
        isChildVisible(learnMore, '#privacyPolicyDiv'), false,
        'privacy policy link should not be visible');

    // Privacy policy iframe does not exist.
    assertEquals(
        isChildVisible(consentStep, '.iframe'), false,
        `privacy policy page should not be visible`);
  });

  test('privacyPolicy', async function() {
    browserProxy.setPrivacySandboxShouldShowPrivacyPolicy(true);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep()!;
    assertEquals(getActiveStep()!.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // The collapse section is opened.
    const learnMore: HTMLElement = consentStep!.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more')!;
    const collapseElement = learnMore!.shadowRoot!.querySelector('cr-collapse');
    testClickButton('cr-expand-button', learnMore);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // Privacy policy div is not shown.
    const learnMoreDiv =
        learnMore!.querySelector<HTMLElement>('#learnMoreDiv')!;
    assertEquals(window.getComputedStyle(learnMoreDiv).display, 'none');

    const privacyPolicyDiv =
        learnMore!.querySelector<HTMLElement>('#privacyPolicyDiv');
    const privacyPolicyLink =
        privacyPolicyDiv!.querySelector<HTMLElement>('#privacyPolicyLink');
    assertTrue(
        !!privacyPolicyLink,
        `the link isn\'t found, selector: ${privacyPolicyDiv}`);
    privacyPolicyLink.click();
    assertEquals(
        isChildVisible(consentStep, '.iframe.visible'), true,
        `privacy policy page should be visible when the link is clicked`);
    assertEquals(
        isChildVisible(consentStep, '#consentNotice'), false,
        `if the privacy policy page is visible,
        the consent notice should not be visible.`);


    // After clicking the back button, the content area should display the
    // consent screen again.
    testClickButton('#backButton', consentStep);
    assertEquals(
        isChildVisible(consentStep, '#confirmButton'), true,
        `buttons should be shown on the consent notice again`);
    assertEquals(
        isChildVisible(consentStep, '.iframe.hidden'), true,
        `privacy policy page should be hidden when the link is clicked`);
  });
});

suite('NoticeEEA', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  function getActiveStep(): PrivacySandboxDialogNoticeStepElement {
    return page.shadowRoot!.querySelector('.active')!;
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

  test('moreButton', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);
    await flushTasks();

    const scrollable: HTMLElement =
        noticeStep.shadowRoot!.querySelector('[scrollable]')!;
    // Turn-off scroll animations.
    scrollable.style.scrollBehavior = 'auto';
    const allContentVisible = isAllContentVisible(scrollable);

    assertEquals(
        isChildVisible(noticeStep, '#moreButton'), !allContentVisible,
        `more button should only be visible when some of the dialog content
        wasn't visible`);

    assertEquals(
        isChildVisible(noticeStep, '#ackButton'), true,
        `ack button should never be hidden`);
    assertEquals(
        isChildInParentBounds(noticeStep, '#ackButton'), allContentVisible,
        allContentVisible ?
            'ack button should visible if all content dialog is visible' :
            `ack button should not be visible if some of the dialog content
            isn't visible from the start`);

    assertEquals(
        isChildVisible(noticeStep, '#settingsButton'), true,
        `settings button should never be hidden`);
    assertEquals(
        isChildInParentBounds(noticeStep, '#settingsButton'), allContentVisible,
        allContentVisible ?
            'settings button should visible if all content dialog is visible' :
            `settings button should not be visible if some of the dialog \
            content isn't visible from the start`);

    if (allContentVisible) {
      return;
    }
    const moreButton: HTMLElement =
        noticeStep.shadowRoot!.querySelector('#moreButton')!;
    moreButton.click();
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_BUTTON_CLICKED);
    await noticeStep.whenWasScrolledToBottomForTest();

    // After scrolling down, the "More" button is hidden and dialog button are
    // visible in the parent bounds.
    assertEquals(
        isChildVisible(noticeStep, '#moreButton'), false,
        'more button should not be visible anymore');
    assertEquals(
        isChildInParentBounds(noticeStep, '#ackButton'), true,
        'ack button should be visible after scrolling to the bottom');
    assertEquals(
        isChildInParentBounds(noticeStep, '#settingsButton'), true,
        'settings button should be visible after scrolling to the bottom');
  });

  test('ackClicked', async function() {
    // Verify that dialog starts with notice step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    // Verify that dialog starts with notice step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep();
    assertEquals(noticeStep!.id, PrivacySandboxCombinedDialogStep.NOTICE);
    // TODO(crbug.com/40244046): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement = noticeStep!.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    const collapseElement =
        learnMoreElement!.shadowRoot!.querySelector('cr-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
  });
});

suite('NoticeROW', function() {
  let page: PrivacySandboxNoticeDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-notice-dialog-app');
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  // TODO(crbug.com/1432915, crbug.com/1432915): various more button test
  // issues. Re-enable once resolved.
  test.skip('moreButton', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    await flushTasks();

    const scrollable: HTMLElement =
        page.shadowRoot!.querySelector('[scrollable]')!;
    // Turn-off scroll animations.
    scrollable.style.scrollBehavior = 'auto';
    const allContentVisible = isAllContentVisible(scrollable);

    assertEquals(
        isChildVisible(page, '#moreButton'), !allContentVisible,
        `more button should only be visible when some of the dialog content
        wasn't visible`);

    assertEquals(
        isChildVisible(page, '#ackButton'), true,
        `ack button should never be hidden`);
    assertEquals(
        isChildInParentBounds(page, '#ackButton'), allContentVisible,
        allContentVisible ?
            'ack button should visible if all content dialog is visible' :
            `ack button should not be visible if some of the dialog content \
            isn't visible from the start`);

    assertEquals(
        isChildVisible(page, '#settingsButton'), true,
        `settings button should never be hidden`);
    assertEquals(
        isChildInParentBounds(page, '#settingsButton'), allContentVisible,
        allContentVisible ?
            'settings button should visible if all content dialog is visible' :
            `settings button should not be visible if some of the dialog \
            content isn't visible from the start`);


    if (allContentVisible) {
      return;
    }
    const moreButton: HTMLElement =
        page.shadowRoot!.querySelector('#moreButton')!;
    moreButton.click();
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_BUTTON_CLICKED);
    await page.whenWasScrolledToBottomForTest();

    // After scrolling down, the "More" button is hidden and dialog button are
    // visible in the parent bounds.
    assertEquals(
        isChildVisible(page, '#moreButton'), false,
        'more button should not be visible anymore');
    assertEquals(
        isChildInParentBounds(page, '#ackButton'), true,
        'ack button should be visible after scrolling to the bottom');
    assertEquals(
        isChildInParentBounds(page, '#settingsButton'), true,
        'settings button should be visible after scrolling to the bottom');
  });

  test('ackClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    testClickButton('#ackButton', page);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    testClickButton('#settingsButton', page);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    // TODO(crbug.com/40244046): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement =
        page.shadowRoot!.querySelector('privacy-sandbox-dialog-learn-more');
    const collapseElement =
        learnMoreElement!.shadowRoot!.querySelector('cr-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area collapses
    // and returns to the initial state.
    testClickButton('cr-expand-button', learnMoreElement);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_INFO_CLOSED);
    assertFalse(collapseElement!.opened);
  });
});

suite('NoticeRestricted', function() {
  let page: PrivacySandboxNoticeRestrictedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page =
        document.createElement('privacy-sandbox-notice-restricted-dialog-app');
    document.body.appendChild(page);
    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('validDialog', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.RESTRICTED_NOTICE_SHOWN);
    assertTrue(!!page.shadowRoot!.querySelector('div'));
  });

  test('settingsClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.RESTRICTED_NOTICE_SHOWN);
    testClickButton('#settingsButton', page);
    await verifyActionOccured(
        browserProxy,
        PrivacySandboxPromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS);
  });

  test('acknowledgeClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.RESTRICTED_NOTICE_SHOWN);
    testClickButton('#ackButton', page);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE);
  });

  // TODO(b/277180533): determine whether some of the more button test logic can
  // be shared.
  // TODO(crbug.com/40903181): various more button test issues. Re-enable once
  // resolved.
  test.skip('moreButton', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.RESTRICTED_NOTICE_SHOWN);
    await flushTasks();

    const scrollable: HTMLElement =
        page.shadowRoot!.querySelector('[scrollable]')!;
    // Turn-off scroll animations.
    scrollable.style.scrollBehavior = 'auto';
    const allContentVisible = isAllContentVisible(scrollable);
    assertEquals(
        isChildVisible(page, '#moreButton'), !allContentVisible,
        `more button should only be visible when some of the dialog content
        wasn't visible`);

    assertEquals(
        isChildVisible(page, '#ackButton'), true,
        `ack button should never be hidden`);
    assertEquals(
        isChildInParentBounds(page, '#ackButton'), allContentVisible,
        allContentVisible ?
            'ack button should visible if all content dialog is visible' :
            `ack button should not be visible if some of the dialog content \
            isn't visible from the start`);

    assertEquals(
        isChildVisible(page, '#settingsButton'), true,
        `settings button should never be hidden`);
    assertEquals(
        isChildInParentBounds(page, '#settingsButton'), allContentVisible,
        allContentVisible ?
            'settings button should visible if all content dialog is visible' :
            `settings button should not be visible if some of the dialog \
            content isn't visible from the start`);

    if (allContentVisible) {
      return;
    }
    const moreButton: HTMLElement =
        page.shadowRoot!.querySelector('#moreButton')!;
    // Click until reaching the bottom.
    while (isChildVisible(page, '#moreButton')) {
      moreButton.click();
      await waitForScrollToFinish(scrollable);
    }

    await verifyActionOccured(
        browserProxy,
        PrivacySandboxPromptAction.RESTRICTED_NOTICE_MORE_BUTTON_CLICKED);
    await page.whenWasScrolledToBottomForTest();

    // After scrolling down, the "More" button is hidden and dialog button are
    // visible in the parent bounds.
    assertEquals(
        isChildVisible(page, '#moreButton'), false,
        'more button should not be visible anymore');
    assertEquals(
        isChildInParentBounds(page, '#ackButton'), true,
        'ack button should be visible after scrolling to the bottom');
    assertEquals(
        isChildInParentBounds(page, '#settingsButton'), true,
        'settings button should be visible after scrolling to the bottom');
  });
});

suite('Mixin', function() {
  const TestElementBase = PrivacySandboxDialogMixin(PolymerElement);

  // Create a test element to have more control over size of the element and
  // test edge cases for 'more' button logic.
  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static get template() {
      return html`
        <style>
          #container {
            width: 500px;
            height: 614px;
            position: relative;
            border: 1px solid red;
          }
          .content-area {
            bottom: 64px;
            position: absolute;
            background: green;
            overflow-y: overlay;
            top: 0;
            width: 100%;
          }
          .buttons-container {
            height: 64px;
            position: fixed;
            box-sizing: border-box;
            bottom: 0;
            right: 0;
            width: 100%;
            background: yellow;
          }
          .more-content-available .buttons-container {
            position: static;
          }
          .more-content-available.content-area {
            bottom: 0;
          }

          .section {
            height: 100px;
            margin: 10px 0px;
            background: blue;
          }
          .section.big {
            height: 200px;
          }
          #showMoreOverlay {
            position: fixed;
            bottom: 0;
            width: 100%;
            height: 64px;
          }
        </style>
        <div id="container">
          <div id="textContent" class="content-area" scrollable>
            <div class="section">Text paragraph 1</div>
            <div class="section big">Text paragraph 2</div>
            <div class="section">Text paragraph 3</div>
            <div id="lastTextElement"  class="section">Text paragraph 4</div>
            <div class="buttons-container">
              Button container
            </div>
          </div>
          <div id="showMoreOverlay" hidden="[[wasScrolledToBottom]]">
            <button id="moreButton" on-click="onMoreClicked_">More</button>
          </div>
        </div>
      `;
    }
  }
  customElements.define(TestElement.is, TestElement);

  let testElement: TestElement;
  let container: HTMLElement;
  let scrollable: HTMLElement;
  let moreButton: HTMLElement;
  let fullContainerHeight: number;

  const LAST_BOTTOM_MARGIN = 10;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
    container = testElement.shadowRoot!.querySelector('#container')!;
    scrollable = testElement.shadowRoot!.querySelector('[scrollable]')!;
    moreButton = testElement.shadowRoot!.querySelector('#moreButton')!;

    const buttons: HTMLElement =
        testElement.shadowRoot!.querySelector('.buttons-container')!;
    fullContainerHeight = scrollable.scrollHeight + buttons.clientHeight;
  });

  test('content not scrollable', async function() {
    container.style.height = `${fullContainerHeight}px`;
    assertFalse(
        doesElemenHaveScrollbar(scrollable),
        'content should not have a scrollbar');

    await testElement.maybeShowMoreButton();
    assertTrue(
        testElement.wasScrolledToBottom, 'last text element should be visible');
  });

  test('only bottom margin of the last element not shown', async function() {
    container.style.height = `${fullContainerHeight - LAST_BOTTOM_MARGIN}px`;
    flush();
    assertTrue(
        doesElemenHaveScrollbar(scrollable), 'content should have a scrollbar');

    await testElement.maybeShowMoreButton();
    assertTrue(
        testElement.wasScrolledToBottom, 'last text element should be visible');
  });

  // The 2 pixels vs 1 pixel choice here is due to intersectionRatio being
  // sometimes reported as 0.99 instead of 1.
  // See more at crbug.com/1020466 and b/299120185.
  test('2px of the last text element not shown', async function() {
    container.style.height =
        `${fullContainerHeight - LAST_BOTTOM_MARGIN - 2}px`;
    assertTrue(
        doesElemenHaveScrollbar(scrollable), 'content should have a scrollbar');

    await testElement.maybeShowMoreButton();
    assertFalse(
        testElement.wasScrolledToBottom, 'last text element wasn\'t shown');

    moreButton.click();
    await testElement.whenWasScrolledToBottomForTest();
    assertTrue(
        testElement.wasScrolledToBottom,
        'last text element was shown after scrolling to the bottom');
  });

  test('last element not shown', async function() {
    container.style.height = `${fullContainerHeight - 100}px`;
    assertTrue(
        doesElemenHaveScrollbar(scrollable), 'content should have a scrollbar');

    await testElement.maybeShowMoreButton();
    assertFalse(
        testElement.wasScrolledToBottom, 'last text element wasn\'t shown');

    moreButton.click();
    await testElement.whenWasScrolledToBottomForTest();
    assertTrue(
        testElement.wasScrolledToBottom,
        'last text element was shown after scrolling to the bottom');
  });

  test('requires more than one "more" button click', async function() {
    const containerHeight = fullContainerHeight / 2 - 50;
    container.style.height = `${containerHeight}px`;
    assertTrue(
        doesElemenHaveScrollbar(scrollable), 'content should have a scrollbar');

    await testElement.maybeShowMoreButton();
    assertFalse(
        testElement.wasScrolledToBottom, 'last text element wasn\'t shown');

    moreButton.click();
    await waitForScrollToFinish(scrollable);
    assertEquals(containerHeight, scrollable.scrollTop);
    assertFalse(
        testElement.wasScrolledToBottom, 'last text element wasn\'t yet shown');

    moreButton.click();
    await testElement.whenWasScrolledToBottomForTest();
    assertTrue(
        testElement.wasScrolledToBottom,
        'last text element was shown after scrolling to the bottom');
  });
});
