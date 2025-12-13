// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_restricted_dialog_app.js';
import 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';

import type {PrivacySandboxCombinedDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';
import {PrivacySandboxCombinedDialogStep} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_combined_dialog_app.js';
import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_browser_proxy.js';
import type {PrivacySandboxDialogConsentStepElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_consent_step.js';
import {PrivacySandboxDialogMixin} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_mixin.js';
import type {PrivacySandboxDialogNoticeStepElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_dialog_notice_step.js';
import type {PrivacySandboxNoticeDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_dialog_app.js';
import type {PrivacySandboxNoticeRestrictedDialogAppElement} from 'chrome://privacy-sandbox-dialog/privacy_sandbox_notice_restricted_dialog_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

class TestPrivacySandboxDialogBrowserProxy extends TestBrowserProxy implements
    PrivacySandboxDialogBrowserProxy {
  private shouldShowAdTopicsContentParity_ = false;

  constructor() {
    super([
      'promptActionOccurred',
      'resizeDialog',
      'showDialog',
      'recordPrivacyPolicyLoadTime',
      'shouldShowAdTopicsContentParity',
    ]);
  }

  setShouldShowAdTopicsContentParity(shouldShow: boolean) {
    this.shouldShowAdTopicsContentParity_ = shouldShow;
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

  shouldShowAdTopicsContentParity() {
    this.methodCalled('shouldShowAdTopicsContentParity');
    return Promise.resolve(this.shouldShowAdTopicsContentParity_);
  }
}

function isChildInParentBounds(
    viewport: HTMLElement, targetSelector: string): boolean {
  const target = viewport.shadowRoot!.querySelector(targetSelector);
  assertTrue(
      !!target, `target element ${targetSelector} not found in the viewport`);
  const targetBounds = target.getBoundingClientRect();
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

function getActiveStep(page: PrivacySandboxCombinedDialogAppElement):
    PrivacySandboxDialogConsentStepElement|
    PrivacySandboxDialogNoticeStepElement {
  return page.shadowRoot!.querySelector('.active')!;
}

suite('Combined', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

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
    const consentStep = getActiveStep(page);
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.CONSENT);
    await consentStep.moreButtonInitializedForTest();
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
    const consentStep = getActiveStep(page);
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Accept the consent step.
    testClickButton('#confirmButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_ACCEPTED);

    // Resolving consent step triggers saving step.
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('acceptConsentAndOpenSettings', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Accept the consent step.
    testClickButton('#confirmButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_ACCEPTED);

    // Resolving consent step triggers saving step.
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Click 'Open settings' button.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('declineConsentAndAckNotice', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Decline the consent step.
    testClickButton('#declineButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_DECLINED);

    // Resolving consent step triggers saving step.
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('declineConsentAndOpenSettings', async function() {
    // Verify that dialog starts with consent step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Decline the consent step.
    testClickButton('#declineButton', consentStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_DECLINED);

    // Resolving consent step triggers saving step.
    assertEquals(
        getActiveStep(page).id, PrivacySandboxCombinedDialogStep.SAVING);

    // After saving step has ended (with a delay), the notice is shown.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Click 'Open settings' button.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  test('learnMoreClicked', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);
    // TODO(crbug.com/40244046): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMoreElement = consentStep.shadowRoot!.querySelector(
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
});

suite('CombinedAdsApiUxEnhancement', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

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

  test('consentEEAContent', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);
    assertFalse(
        isVisible(consentStep.shadowRoot!.querySelector('#consentContent')));
    assertTrue(
        isVisible(consentStep.shadowRoot!.querySelector('#consentContentV2')));
  });

  test('consentEEAPrivacyPolicy', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Privacy policy page is initially not loaded.
    let privacyPolicyDialog = consentStep.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertFalse(!!privacyPolicyDialog);

    // The collapse section is opened.
    const learnMore = consentStep.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    assertTrue(!!learnMore);
    const collapseElement = learnMore.shadowRoot!.querySelector('cr-collapse');
    testClickButton('cr-expand-button', learnMore);
    await waitAfterNextRender(learnMore);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);
    assertFalse(
        isVisible(consentStep.shadowRoot!.querySelector('#privacyPolicyLink')));
    assertFalse(
        isVisible(consentStep.shadowRoot!.querySelector('#privacyPolicyDiv')));
    assertFalse(isVisible(consentStep.shadowRoot!.querySelector(
        '#learnMoreBulletDescriptionContentParity')));
    assertFalse(isVisible(consentStep.shadowRoot!.querySelector(
        '#learnMoreBulletDescriptionNoLink')));
    assertTrue(isVisible(
        consentStep.shadowRoot!.querySelector('#learnMoreBulletDescription')));
    const privacyPolicyLinkV2 =
        consentStep.shadowRoot!.querySelector<HTMLElement>(
            '#privacyPolicyLinkV2');
    assertTrue(!!privacyPolicyLinkV2);
    assertTrue(isVisible(privacyPolicyLinkV2));
    privacyPolicyLinkV2.click();
    await microtasksFinished();

    privacyPolicyDialog = consentStep.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertTrue(!!privacyPolicyDialog);
    const privacyPolicy =
        privacyPolicyDialog.shadowRoot.querySelector('#privacyPolicy');
    assertTrue(!!privacyPolicy);
    const privacyPolicyBackButtonContainer =
        privacyPolicyDialog.shadowRoot.querySelector('.button-container');
    assertTrue(!!privacyPolicyBackButtonContainer);

    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '1',
        `privacy policy page should be visible when the link is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'flex',
        `privacy policy back button should be visible when the link is clicked`);
    assertEquals(
        isChildVisible(consentStep, '#consentNotice'), false,
        `if the privacy policy page is visible,
        the consent notice should not be visible.`);

    // After clicking the back button, the content area should display the
    // consent screen again.
    testClickButton('#backButton', privacyPolicyDialog);
    await microtasksFinished();

    assertEquals(
        isChildVisible(consentStep, '#confirmButton'), true,
        `buttons should be shown on the consent notice again`);
    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '0',
        `privacy policy page should be hidden when the back button is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'none',
        `privacy policy back button should be hidden when the back button is clicked`);
  });
});

suite('CombinedAdsApiUxEnhancementAdTopicsContentParity', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

  setup(async function() {
    browserProxy = new TestPrivacySandboxDialogBrowserProxy();
    browserProxy.setShouldShowAdTopicsContentParity(true);
    PrivacySandboxDialogBrowserProxy.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('privacy-sandbox-combined-dialog-app');
    page.disableAnimationsForTesting();
    document.body.appendChild(page);

    await browserProxy.whenCalled('resizeDialog');
    await browserProxy.whenCalled('showDialog');
  });

  test('consentEEAContent', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);
    assertFalse(
        isVisible(consentStep.shadowRoot!.querySelector('#consentContent')));
    assertTrue(
        isVisible(consentStep.shadowRoot!.querySelector('#consentContentV2')));
    const consentContentV2FirstDescription =
        consentStep.shadowRoot!.querySelector<HTMLElement>(
            '#consentContentV2FirstDescription');
    assertTrue(isVisible(consentContentV2FirstDescription));
    assertEquals(
        loadTimeData.getString('m1ConsentDescription1ContentParity'),
        consentContentV2FirstDescription!.innerText);
  });

  test('consentEEAPrivacyPolicy', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_SHOWN);
    const consentStep = getActiveStep(page);
    assertEquals(consentStep.id, PrivacySandboxCombinedDialogStep.CONSENT);

    // Privacy policy page is initially not loaded.
    let privacyPolicyDialog = consentStep.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertFalse(!!privacyPolicyDialog);

    // The collapse section is opened.
    const learnMore = consentStep.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    assertTrue(!!learnMore);
    const collapseElement = learnMore.shadowRoot!.querySelector('cr-collapse');
    testClickButton('cr-expand-button', learnMore);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);
    await waitAfterNextRender(learnMore);
    assertFalse(
        isVisible(consentStep.shadowRoot!.querySelector('#privacyPolicyLink')));
    assertFalse(isVisible(
        consentStep.shadowRoot!.querySelector('#privacyPolicyLinkV2')));
    assertFalse(
        isVisible(consentStep.shadowRoot!.querySelector('#privacyPolicyDiv')));
    // Testing Ad Topics Content Parity Changes
    assertFalse(isVisible(
        consentStep.shadowRoot!.querySelector('#learnMoreBulletDescription')));
    const learnMoreBulletDescriptionContentParity =
        consentStep.shadowRoot!.querySelector<HTMLElement>(
            '#learnMoreBulletDescriptionContentParity');
    assertTrue(isVisible(learnMoreBulletDescriptionContentParity));
    const learnMoreBulletDescriptionContentParityText =
        loadTimeData
            .getString('m1ConsentLearnMoreBullet2DescriptionContentParity')
            .replace(/<[^>]*>/g, '')  // Remove HTML tags
            .trim();
    assertEquals(
        learnMoreBulletDescriptionContentParityText,
        learnMoreBulletDescriptionContentParity!.innerText.trim());
    assertFalse(isVisible(consentStep.shadowRoot!.querySelector(
        '#learnMoreBulletDescriptionNoLink')));
    const privacyPolicyLinkV3 =
        consentStep.shadowRoot!.querySelector<HTMLElement>(
            '#privacyPolicyLinkV3');
    assertTrue(!!privacyPolicyLinkV3);
    assertTrue(isVisible(privacyPolicyLinkV3));
    privacyPolicyLinkV3.click();
    await microtasksFinished();

    privacyPolicyDialog = consentStep.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertTrue(!!privacyPolicyDialog);
    const privacyPolicy =
        privacyPolicyDialog.shadowRoot.querySelector('#privacyPolicy');
    assertTrue(!!privacyPolicy);
    const privacyPolicyBackButtonContainer =
        privacyPolicyDialog.shadowRoot.querySelector('.button-container');
    assertTrue(!!privacyPolicyBackButtonContainer);

    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '1',
        `privacy policy page should be visible when the link is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'flex',
        `privacy policy back button should be visible when the link is clicked`);
    assertEquals(
        isChildVisible(consentStep, '#consentNotice'), false,
        `if the privacy policy page is visible,
        the consent notice should not be visible.`);

    // After clicking the back button, the content area should display the
    // consent screen again.
    testClickButton('#backButton', privacyPolicyDialog);
    await microtasksFinished();

    assertEquals(
        isChildVisible(consentStep, '#confirmButton'), true,
        `buttons should be shown on the consent notice again`);
    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '0',
        `privacy policy page should be hidden when the back button is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'none',
        `privacy policy back button should be hidden when the back button is clicked`);
  });
});


suite('NoticeEEAAdsApiUxEnhancement', function() {
  let page: PrivacySandboxCombinedDialogAppElement;
  let browserProxy: TestPrivacySandboxDialogBrowserProxy;

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
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);
    await noticeStep.moreButtonInitializedForTest();
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
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);

    // Acknowledge the notice.
    testClickButton('#ackButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  });

  test('settingsClicked', async function() {
    // Verify that dialog starts with notice step.
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);
    await noticeStep.moreButtonInitializedForTest();

    // Acknowledge the notice.
    testClickButton('#settingsButton', noticeStep);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  });

  async function verifyCollapseSectionOpensAndCloses(
      learnMoreELementId: string, openedAction: PrivacySandboxPromptAction,
      closedAction: PrivacySandboxPromptAction,
      browserProxy: TestPrivacySandboxDialogBrowserProxy) {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);
    // TODO(crbug.com/40244046): Test scrolling behaviour.
    // The collapse section is closed.
    const learnMore =
        noticeStep.shadowRoot!.querySelector<HTMLElement>(learnMoreELementId);
    const collapseElement = learnMore!.shadowRoot!.querySelector('cr-collapse');
    assertFalse(collapseElement!.opened);

    // The collapse section is opened and the native UI is notified about the
    // action.
    testClickButton('cr-expand-button', learnMore);
    await verifyActionOccured(browserProxy, openedAction);
    assertTrue(collapseElement!.opened);

    // After clicking on the collapse section again, the content area
    // collapses and returns to the initial state.
    testClickButton('cr-expand-button', learnMore);
    await verifyActionOccured(browserProxy, closedAction);
    assertFalse(collapseElement!.opened);
  }

  test('siteSuggestedAdsLearnMoreClicked', function() {
    verifyCollapseSectionOpensAndCloses(
        '#siteSuggestedAdsLearnMore',
        PrivacySandboxPromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_OPENED,
        PrivacySandboxPromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_CLOSED,
        browserProxy);
  });

  test('adsMeasurementLearnMoreClicked', function() {
    verifyCollapseSectionOpensAndCloses(
        '#adsMeasurementLearnMore',
        PrivacySandboxPromptAction.NOTICE_ADS_MEASUREMENT_MORE_INFO_OPENED,
        PrivacySandboxPromptAction.NOTICE_ADS_MEASUREMENT_MORE_INFO_CLOSED,
        browserProxy);
  });

  test('noticeEEAContent', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);
    assertFalse(
        isVisible(noticeStep.shadowRoot!.querySelector('#noticeContent')));
    assertTrue(
        isVisible(noticeStep.shadowRoot!.querySelector('#noticeContentV2')));
  });

  test('privacyPolicyShown', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    const noticeStep = getActiveStep(page);
    assertEquals(noticeStep.id, PrivacySandboxCombinedDialogStep.NOTICE);

    let privacyPolicyDialog = noticeStep.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertFalse(!!privacyPolicyDialog);

    // The collapse section is opened.
    const learnMore = noticeStep.shadowRoot!.querySelector(
        'privacy-sandbox-dialog-learn-more');
    assertTrue(!!learnMore);
    const collapseElement = learnMore.shadowRoot!.querySelector('cr-collapse');
    testClickButton('cr-expand-button', learnMore);
    await flushTasks();
    await verifyActionOccured(
        browserProxy,
        PrivacySandboxPromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    const privacyPolicyLinkV2 =
        noticeStep.shadowRoot!.querySelector<HTMLElement>(
            '#privacyPolicyLinkV2');
    assertTrue(!!privacyPolicyLinkV2);
    assertTrue(isVisible(privacyPolicyLinkV2));
    privacyPolicyLinkV2.click();
    await microtasksFinished();

    privacyPolicyDialog = noticeStep.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertTrue(!!privacyPolicyDialog);
    const privacyPolicy =
        privacyPolicyDialog.shadowRoot.querySelector('#privacyPolicy');
    assertTrue(!!privacyPolicy);
    const privacyPolicyBackButtonContainer =
        privacyPolicyDialog.shadowRoot.querySelector('.button-container');
    assertTrue(!!privacyPolicyBackButtonContainer);

    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '1',
        `privacy policy page should be visible when the link is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'flex',
        `privacy policy back button should be visible when the link is clicked`);
    assertEquals(
        isChildVisible(noticeStep, '#notice'), false,
        `if the privacy policy page is visible,
        the consent notice should not be visible.`);

    // After clicking the back button, the content area should display the
    // consent screen again.
    testClickButton('#backButton', privacyPolicyDialog);
    await microtasksFinished();

    assertEquals(
        isChildVisible(noticeStep, '#ackButton'), true,
        `buttons should be shown on the consent notice again`);
    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '0',
        `privacy policy page should be hidden when the back button is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'none',
        `privacy policy back button should be hidden when the back button is clicked`);
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
  test('moreButton', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    await page.moreButtonInitializedForTest();
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

    /* These assertions fail on ChromeOS.
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
    */

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

suite('NoticeROWAdsApiUxEnhancement', function() {
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

  test('learnMoreAndLastText', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('#learnMoreAndLastText')));
    assertTrue(
        isVisible(page.shadowRoot!.querySelector('#learnMoreAndLastTextV2')));
  });

  test('privacyPolicyShown', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_SHOWN);

    let privacyPolicyDialog = page!.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertFalse(!!privacyPolicyDialog);

    // The collapse section is opened.
    const learnMore =
        page.shadowRoot!.querySelector('privacy-sandbox-dialog-learn-more');
    assertTrue(!!learnMore);
    const collapseElement = learnMore.shadowRoot!.querySelector('cr-collapse');
    testClickButton('cr-expand-button', learnMore);
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.NOTICE_MORE_INFO_OPENED);
    assertTrue(collapseElement!.opened);

    const privacyPolicyLinkV2 =
        page.shadowRoot!.querySelector<HTMLElement>('#privacyPolicyLinkV2');
    assertTrue(!!privacyPolicyLinkV2);
    assertTrue(isVisible(privacyPolicyLinkV2));
    privacyPolicyLinkV2.click();
    await microtasksFinished();

    privacyPolicyDialog = page!.shadowRoot!.querySelector(
        'privacy-sandbox-privacy-policy-dialog');
    assertTrue(!!privacyPolicyDialog);
    const privacyPolicy =
        privacyPolicyDialog.shadowRoot.querySelector('#privacyPolicy');
    assertTrue(!!privacyPolicy);
    const privacyPolicyBackButtonContainer =
        privacyPolicyDialog.shadowRoot.querySelector('.button-container');
    assertTrue(!!privacyPolicyBackButtonContainer);

    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '1',
        `privacy policy page should be visible when the link is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'flex',
        `privacy policy back button should be visible when the link is clicked`);
    assertEquals(
        isChildVisible(page, '#notice'), false,
        `if the privacy policy page is visible,
        the consent notice should not be visible.`);

    // After clicking the back button, the content area should display the
    // consent screen again.
    testClickButton('#backButton', privacyPolicyDialog);
    await microtasksFinished();

    assertEquals(
        isChildVisible(page, '#ackButton'), true,
        `buttons should be shown on the consent notice again`);
    assertEquals(
        getComputedStyle(privacyPolicy).opacity, '0',
        `privacy policy page should be hidden when the back button is clicked`);
    assertEquals(
        getComputedStyle(privacyPolicyBackButtonContainer).display, 'none',
        `privacy policy back button should be hidden when the back button is clicked`);
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
  test('moreButton', async function() {
    await verifyActionOccured(
        browserProxy, PrivacySandboxPromptAction.RESTRICTED_NOTICE_SHOWN);
    await page.moreButtonInitializedForTest();
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
