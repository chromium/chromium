// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, whenCheck} from 'chrome-untrusted://webui-test/test_util.js';

function deepQuerySelector(root: Element, selector: string): HTMLElement|null {
  const currentRoot = root.shadowRoot ? root.shadowRoot : root;
  const match = currentRoot.querySelector(selector);
  if (match) {
    return match as HTMLElement;
  }
  const elements = currentRoot.querySelectorAll('*');
  for (const element of elements) {
    const found = deepQuerySelector(element, selector);
    if (found) {
      return found;
    }
  }
  return null;
}

function getHtmlElement(root: Element, selector: string): HTMLElement {
  const element = deepQuerySelector(root, selector);
  assertTrue(!!element);
  return element!;
}

function completePendingMicrotasks() {
  return new Promise((resolve) => void setTimeout(resolve, 0));
}

async function maybeDismissSplashDialog(app: HTMLElement) {
  const dialog = deepQuerySelector(app, 'gemini-splash-dialog');
  if (!dialog) {
    return;
  }
  const innerDialog = getHtmlElement(dialog, 'mwc-dialog');
  const closedPromise = eventToPromise('closed', innerDialog);
  const closeButton = getHtmlElement(dialog, '#close-button');
  eventToPromise('opened', innerDialog).then(() => closeButton.click());
  // An extra click in case the dialog was fully opened at this point, in this
  // case the opened event won't be caught in the `eventToPromise` above.
  closeButton.click();
  await closedPromise;
  await completePendingMicrotasks();
}

async function openGeminiDialog(app: HTMLElement):
    Promise<{geminiDialog: HTMLElement, geminiInnerDialog: HTMLElement}> {
  const addResourceMenu = getHtmlElement(app, '#add-resource-menu');
  (getHtmlElement(addResourceMenu, 'md-menu') as unknown as any)
      .stayOpenOnFocusout = true;
  const resourceMenuOpenedPromise = eventToPromise('opened', addResourceMenu);
  getHtmlElement(app, '#add-resource-button').click();
  await resourceMenuOpenedPromise;
  await completePendingMicrotasks();
  const geminiDialog = getHtmlElement(app, 'gemini-settings-dialog');
  const geminiInnerDialog = getHtmlElement(geminiDialog, 'mwc-dialog');
  const dialogOpenedPromise = eventToPromise('opened', geminiInnerDialog);
  const geminiMenuItem = getHtmlElement(app, '#gemini-menu-item');
  getHtmlElement(geminiMenuItem, '.list-item').click();
  await dialogOpenedPromise;
  await completePendingMicrotasks();
  return {geminiDialog, geminiInnerDialog};
}

async function startSession(app: HTMLElement) {
  const selectDurationDialog = getHtmlElement(app, 'select-duration-dialog');
  const selectDurationInnerDialog =
      getHtmlElement(selectDurationDialog, 'mwc-dialog');
  const selectDurationOpenedPromise =
      eventToPromise('opened', selectDurationInnerDialog);
  const selectDurationClosedPromise =
      eventToPromise('closed', selectDurationInnerDialog);
  const sessionControlWidget = getHtmlElement(app, 'session-control-widget');
  getHtmlElement(sessionControlWidget, 'cros-button[button-style="primary"]')
      .click();
  await selectDurationOpenedPromise;
  getHtmlElement(selectDurationDialog, '#confirm-button').click();
  await selectDurationClosedPromise;
  // Wait for session to start
  const teacherView = getHtmlElement(app, 'teacher-view');
  await whenCheck(
      teacherView, () => !!deepQuerySelector(teacherView, 'access-code-view'));
}

suite('CreateSession', function() {
  test('WithGeminiGuidedLearning', async () => {
    const app = document.querySelector<HTMLElement>('boca-app')!;
    assertTrue(!!app);
    await maybeDismissSplashDialog(app);
    // Open content subpage
    const contentWidget = getHtmlElement(app, 'content-widget');
    getHtmlElement(contentWidget, 'cros-card').click();
    await completePendingMicrotasks();
    // Add the Gemini resource
    const {geminiDialog, geminiInnerDialog} = await openGeminiDialog(app);
    const geminiDialogClosedPromise =
        eventToPromise('closed', geminiInnerDialog);
    getHtmlElement(geminiDialog, '#confirm-button').click();
    await geminiDialogClosedPromise;
    await completePendingMicrotasks();
    // Go back to main page and start session
    getHtmlElement(app, '#back-button').click();
    await completePendingMicrotasks();
    await startSession(app);
  });
});
