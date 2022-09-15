// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://resources/cr_components/help_bubble/help_bubble.js';

import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {HELP_BUBBLE_DISMISSED_EVENT, HELP_BUBBLE_TIMED_OUT_EVENT, HelpBubbleDismissedEvent, HelpBubbleElement, HelpBubbleTimedOutEvent} from 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import {HelpBubbleArrowPosition, HelpBubbleButtonParams} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

interface WaitForSuccessParams {
  retryIntervalMs: number;
  totalMs: number;
  assertionFn: () => void;
}

suite('CrComponentsHelpBubbleTest', () => {
  let helpBubble: HelpBubbleElement;

  function getNumButtons() {
    return helpBubble.$.buttons.querySelectorAll('[id*="action-button"').length;
  }

  function getProgressIndicators() {
    return helpBubble.$.progress.querySelectorAll('[class*="progress"]');
  }

  /**
   * Finds and returns an element with class `name` that can be in the
   * topContainer or in the main container of the HelpBubbleElement. If
   * `inTopContainer` is true, will locate the element in the top container and
   * fail the test if the element is (also) visible in the main body. If
   * `inTopContainer` is false, the reverse applies.
   */
  function getMovableElement(
      name: string, inTopContainer: boolean): HTMLElement {
    const query = `.${name}`;
    const headerEl =
        helpBubble.$.topContainer.querySelector<HTMLElement>(query);
    const mainEl = helpBubble.$.main.querySelector<HTMLElement>(query);
    assertTrue(
        !!headerEl,
        `getMovableElement - ${query} element should exist in header`);
    assertTrue(
        !!mainEl, `getMovableElement - ${query} element should exist in main`);
    if (inTopContainer) {
      assertFalse(
          isVisible(mainEl),
          'getMovableElement - element in main should not be visible');
      return headerEl;
    }
    assertFalse(
        isVisible(headerEl),
        'getMovableElement - element in header should not be visible');
    return mainEl;
  }

  /**
   * Create a promise that resolves after a given amount of time
   */
  async function sleep(milliseconds: number) {
    return new Promise((res) => {
      setTimeout(res, milliseconds);
    });
  }

  /**
   * Returns the current timestamp in milliseconds since UNIX epoch
   */
  function now() {
    return +new Date();
  }

  /**
   * Try/catch a function for some time, retrying after failures
   *
   * If the callback function succeeds, return early with the total time
   * If the callback always fails, throw the error after the last run
   */
  async function waitForSuccess(params: WaitForSuccessParams):
      Promise<number|null> {
    const startMs = now();
    let lastAttemptMs = startMs;
    let lastError: Error|null = null;
    let attempts = 0;
    while (now() - startMs < params.totalMs) {
      await sleep(params.retryIntervalMs);
      lastAttemptMs = now();
      try {
        params.assertionFn();
        return lastAttemptMs - startMs;
      } catch (e) {
        lastError = e as Error;
      }
      attempts++;
    }
    if (lastError !== null) {
      lastError.message = `[Attempts: ${attempts}, Total time: ${
          lastAttemptMs - startMs}ms]: ${lastError.message}`;
      throw lastError;
    }
    return Infinity;
  }

  setup(() => {
    document.body.innerHTML = `
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='p1'>Some paragraph text</p>
      <ul id='bulletList'>
        <li>List item 1</li>
        <li>List item 2</li>
      </ul>
    </div>`;

    helpBubble = document.createElement('help-bubble');
    helpBubble.id = 'helpBubble';
    document.querySelector<HTMLElement>('#container')!.insertBefore(
        helpBubble, document.querySelector<HTMLElement>('#bulletList'));
  });

  test('bubble starts closed wth no anchor', () => {
    assertEquals(
        null, helpBubble.getAnchorElement(),
        'help bubble should have no anchor');
  });

  const HELP_BUBBLE_BODY = 'help bubble body';
  const HELP_BUBBLE_TITLE = 'help bubble title';

  test('help bubble shows and anchors correctly', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();

    assertEquals(
        document.querySelector<HTMLElement>('#p1'),
        helpBubble.getAnchorElement(),
        'help bubble should have correct anchor element');
    const bodyElement = getMovableElement('body', true);
    assertFalse(bodyElement.hidden, 'body should not be hidden');
    assertEquals(
        HELP_BUBBLE_BODY, bodyElement.textContent!.trim(),
        'body content show match');
    assertTrue(isVisible(helpBubble), 'help bubble should be visible');
  });

  test('help bubble titles shows', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.titleText = HELP_BUBBLE_TITLE;
    helpBubble.show();

    assertTrue(isVisible(helpBubble), 'help bubble should be visible');
    const titleElement =
        helpBubble.$.topContainer.querySelector<HTMLElement>('.title');
    assertTrue(!!titleElement, 'title element should exist');
    assertFalse(titleElement.hidden, 'title element should not be hidden');
    assertEquals(
        HELP_BUBBLE_TITLE, titleElement.textContent!.trim(),
        'title content should match');
    assertTrue(isVisible(titleElement), 'title element should be visible');
  });

  test('help bubble titles hides when no title set', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();

    assertTrue(isVisible(helpBubble), 'help bubble should be visible');
    const titleElement =
        helpBubble.$.topContainer.querySelector<HTMLElement>('.title');
    assertTrue(!!titleElement, 'title element should exist');
    assertTrue(titleElement.hidden, 'title element should be hidden');
  });

  test('help bubble body icon shows when set', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.bodyIconName = 'icon_name';
    helpBubble.bodyIconAltText = '';
    helpBubble.show();

    assertTrue(isVisible(helpBubble), 'help bubble should be visible');
    const bodyIcon = helpBubble.$.bodyIcon;
    assertTrue(!!bodyIcon, 'body icon element should exist');
    assertFalse(bodyIcon.hidden, 'body icon element should not be hidden');
    assertTrue(isVisible(bodyIcon), 'body icon element should be visible');
    assertEquals(
        bodyIcon.querySelector<IronIconElement>('iron-icon')!.icon,
        'iph:icon_name',
        'bodyIcon passes icon name to iron-icon with iph namespace');
  });

  test('help bubble body icon is hidden when null', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.bodyIconName = null;
    helpBubble.bodyIconAltText = '';
    helpBubble.show();

    assertTrue(isVisible(helpBubble), 'help bubble should be visible');
    const bodyIcon = helpBubble.$.bodyIcon;
    assertTrue(!!bodyIcon, 'body icon element should exist');
    assertTrue(bodyIcon.hidden, 'body icon element should be hidden');
    assertFalse(isVisible(bodyIcon), 'body icon element should not be visible');
  });

  test('help bubble closes', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();

    assertEquals(
        document.querySelector<HTMLElement>('#title'),
        helpBubble.getAnchorElement(),
        'title element should be anchor element');

    helpBubble.hide();
    assertEquals(
        null, helpBubble.getAnchorElement(),
        'help bubble should not have anchor');
    assertFalse(isVisible(helpBubble), 'help bubble should not be visible');
  });

  test('help bubble open close open', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();
    helpBubble.hide();
    helpBubble.show();
    assertEquals(
        document.querySelector<HTMLElement>('#title'),
        helpBubble.getAnchorElement(),
        'title element should be anchor element');
    const bodyElement = getMovableElement('body', true);
    assertFalse(bodyElement.hidden, 'body element should not be hidden');
    assertEquals(
        HELP_BUBBLE_BODY, bodyElement.textContent!.trim(),
        'body content should match');
    assertTrue(isVisible(helpBubble), 'help bubble should be visible');
  });

  test('help bubble close button has correct alt text', () => {
    const CLOSE_TEXT: string = 'Close button text.';
    const ICON_TEXT: string = 'Body icon text.';
    helpBubble.closeButtonAltText = CLOSE_TEXT;
    helpBubble.bodyIconAltText = ICON_TEXT;
    assertEquals(
        CLOSE_TEXT, helpBubble.$.close.getAttribute('aria-label'),
        'close button should have aria-label content');
    assertEquals(
        ICON_TEXT, helpBubble.$.bodyIcon.getAttribute('aria-label'),
        'body icon should have aria-label content');
  });

  test('help bubble click close button generates event', async () => {
    let clicked: number = 0;
    const callback = (e: HelpBubbleDismissedEvent) => {
      assertEquals(
          'title', e.detail.anchorId, 'dismiss event anchorId should match');
      assertFalse(
          e.detail.fromActionButton,
          'dismiss event should not be from action button');
      ++clicked;
    };
    helpBubble.addEventListener(HELP_BUBBLE_DISMISSED_EVENT, callback);
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    const closeButton = helpBubble.$.close;
    assertEquals(0, clicked, 'close button should not be clicked');
    closeButton.click();
    assertEquals(1, clicked, 'close button should be clicked once');
  });

  test('help bubble with timeout does not immediately emit event', async () => {
    let timedOut: number = 0;
    const callback = (e: HelpBubbleTimedOutEvent) => {
      assertEquals(
          'title', e.detail.anchorId, 'timeout event anchorId should match');
      ++timedOut;
    };
    helpBubble.addEventListener(HELP_BUBBLE_TIMED_OUT_EVENT, callback);
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.timeoutMs = 10 * 1000;  // 10s
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    assertEquals(0, timedOut, 'timeout should not be triggered');
  });

  test('help bubble with timeout generates event', async () => {
    const timeoutMs: number = 100;
    let timedOut: number = 0;
    const callback = (e: HelpBubbleTimedOutEvent) => {
      assertEquals(
          'title', e.detail.anchorId, 'timeout event anchorId should match');
      ++timedOut;
    };
    helpBubble.addEventListener(HELP_BUBBLE_TIMED_OUT_EVENT, callback);
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.timeoutMs = timeoutMs;  // 100ms
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    await waitForSuccess({
      retryIntervalMs: 50,
      totalMs: 1500,
      assertionFn: () => assertEquals(1, timedOut, 'timeout should emit event'),
    }) as number;
  });

  test('help bubble without timeout does not generate event', async () => {
    let timedOut: number = 0;
    const callback = (e: HelpBubbleTimedOutEvent) => {
      assertEquals(
          'title', e.detail.anchorId, 'timeout event anchorId should match');
      ++timedOut;
    };
    helpBubble.addEventListener(HELP_BUBBLE_TIMED_OUT_EVENT, callback);
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();
    assertEquals(0, timedOut, 'timeout should not be triggered');
    await waitAfterNextRender(helpBubble);
    await sleep(100);  // 100ms
    assertEquals(0, timedOut, 'timeout is never triggered');
  });

  test('help bubble adds one button', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = [{text: 'button1', isDefault: false}];
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    assertEquals(1, getNumButtons(), 'there should be one button');
    const button = helpBubble.getButtonForTesting(0);
    assertTrue(!!button, 'button should exist');
    assertEquals(
        helpBubble.buttons[0]!.text, button.textContent,
        'help bubble button content should match');
    assertFalse(
        button.classList.contains('default-button'),
        'button should not have default class');
  });

  test('help bubble adds several buttons', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = [
      {text: 'button1', isDefault: false},
      {text: 'button2', isDefault: false},
      {text: 'button3', isDefault: false},
    ];
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    assertEquals(3, getNumButtons(), 'there should be three buttons');
    for (let i: number = 0; i < 3; ++i) {
      const button = helpBubble.getButtonForTesting(i);
      assertTrue(!!button, 'button should exist');
      assertEquals(
          helpBubble.buttons[i]!.text, button.textContent,
          'button content should match');
      assertFalse(
          button.classList.contains('default-button'),
          'button should not have default class');
    }
  });

  test('help bubble adds default button', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = [{text: 'button1', isDefault: true}];
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    const button = helpBubble.getButtonForTesting(0);
    assertTrue(!!button, 'button should exist');
    assertTrue(
        button.classList.contains('default-button'),
        'button should have default class');
  });

  const THREE_BUTTONS_MIDDLE_DEFAULT: HelpBubbleButtonParams[] = [
    {text: 'button1', isDefault: false},
    {text: 'button2', isDefault: true},
    {text: 'button3', isDefault: false},
  ];

  test('help bubble adds default button among several', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = THREE_BUTTONS_MIDDLE_DEFAULT;
    helpBubble.show();
    await waitAfterNextRender(helpBubble);
    assertEquals(3, getNumButtons(), 'there should be three buttons');

    // Make sure all buttons were created as expected, including the default
    // button.
    let defaultButton: CrButtonElement|null = null;
    for (let i: number = 0; i < 3; ++i) {
      const button = helpBubble.getButtonForTesting(i);
      assertTrue(!!button, 'button should exist');
      assertEquals(
          helpBubble.buttons[i]!.text, button.textContent,
          'button content should match');
      const isDefault = helpBubble.buttons[i]!.isDefault;
      assertEquals(
          isDefault, button.classList.contains('default-button'),
          `button should ${isDefault ? '' : 'not '}have have default class`);
      if (isDefault) {
        defaultButton = button;
      }
    }

    // Verify that the default button is in the expected position.
    assertTrue(!!defaultButton, 'default button should exist');
    const expectedIndex = HelpBubbleElement.isDefaultButtonLeading() ? 0 : 2;
    assertEquals(
        defaultButton, helpBubble.$.buttons.children.item(expectedIndex),
        'default button should be at correct index');
  });

  test('help bubble click action button generates event', async () => {
    let clicked: boolean;
    let buttonIndex: number;
    const callback = (e: HelpBubbleDismissedEvent) => {
      assertEquals('title', e.detail.anchorId, 'Check anchor.');
      assertTrue(e.detail.fromActionButton, 'Check fromActionButton.');
      assertTrue(e.detail.buttonIndex !== undefined, 'Check buttonIndex.');
      clicked = true;
      buttonIndex = e.detail.buttonIndex;
    };
    helpBubble.addEventListener(HELP_BUBBLE_DISMISSED_EVENT, callback);
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = THREE_BUTTONS_MIDDLE_DEFAULT;

    for (let i: number = 0; i < 3; ++i) {
      clicked = false;
      buttonIndex = -1;
      helpBubble.show();
      await waitAfterNextRender(helpBubble);
      const button = helpBubble.getButtonForTesting(i);
      assertTrue(!!button, 'button should exist');
      button.click();
      assertTrue(clicked, 'button should be clicked');
      assertEquals(i, buttonIndex, 'button index should match');
      helpBubble.hide();
    }
  });

  test('help bubble with no progress doesn\'t show progress', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = THREE_BUTTONS_MIDDLE_DEFAULT;

    helpBubble.show();
    await waitAfterNextRender(helpBubble);

    assertEquals(
        0, getProgressIndicators().length,
        'there should not be progress indicators');
    const bodyElement = getMovableElement('body', true);
    assertFalse(bodyElement.hidden, 'body element should not be hidden');
  });

  test(
      'help bubble with no progress and title doesn\'t show progress',
      async () => {
        helpBubble.anchorId = 'title';
        helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
        helpBubble.bodyText = HELP_BUBBLE_BODY;
        helpBubble.titleText = HELP_BUBBLE_TITLE;
        helpBubble.buttons = THREE_BUTTONS_MIDDLE_DEFAULT;

        helpBubble.show();
        await waitAfterNextRender(helpBubble);

        assertEquals(
            0, getProgressIndicators().length,
            'there should not be progress indicators');
        const titleElement =
            helpBubble.$.topContainer.querySelector<HTMLElement>('.title');
        assertTrue(!!titleElement, 'title element should exist');
        assertFalse(titleElement.hidden, 'title element should not be hidden');
        assertFalse(
            getMovableElement('body', false).hidden,
            'body element should not be hidden');
      });

  test('help bubble with progress shows progress', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.progress = {current: 1, total: 3};
    helpBubble.buttons = THREE_BUTTONS_MIDDLE_DEFAULT;

    helpBubble.show();
    await waitAfterNextRender(helpBubble);

    const elements = getProgressIndicators();
    assertEquals(3, elements.length, 'there should be three elements');
    assertTrue(
        elements.item(0)!.classList.contains('current-progress'),
        'element 0 should have current-progress class');
    assertTrue(
        elements.item(1)!.classList.contains('total-progress'),
        'element 1 should have total-progress class');
    assertTrue(
        elements.item(2)!.classList.contains('total-progress'),
        'element 2 should have total-progress class');
    assertFalse(getMovableElement('body', false).hidden);
  });

  test('help bubble with progress and title shows progress', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.titleText = HELP_BUBBLE_TITLE;
    helpBubble.progress = {current: 1, total: 2};
    helpBubble.buttons = THREE_BUTTONS_MIDDLE_DEFAULT;

    helpBubble.show();
    await waitAfterNextRender(helpBubble);

    const elements = getProgressIndicators();
    assertEquals(2, elements.length, 'there should be two elements');
    assertTrue(
        elements.item(0)!.classList.contains('current-progress'),
        'element 0 should have current-progress class');
    assertTrue(
        elements.item(1)!.classList.contains('total-progress'),
        'element 1 should have total-progress class');

    const titleElement =
        helpBubble.$.topContainer.querySelector<HTMLElement>('.title');
    assertTrue(!!titleElement, 'title element should exist');
    assertTrue(titleElement.hidden, 'title element should be hidden');
    assertFalse(
        getMovableElement('body', false).hidden,
        'body element should not be hidden');
  });

  test('help bubble with full progress', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.progress = {current: 2, total: 2};

    helpBubble.show();
    await waitAfterNextRender(helpBubble);

    const elements = getProgressIndicators();
    assertEquals(2, elements.length, 'there should be two elements');
    assertTrue(
        elements.item(0)!.classList.contains('current-progress'),
        'element 0 should have current-progress class');
    assertTrue(
        elements.item(1)!.classList.contains('current-progress'),
        'element 1 should have current-progress class');
  });

  test('help bubble with empty progress', async () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubbleArrowPosition.TOP_CENTER;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.progress = {current: 0, total: 2};

    helpBubble.show();
    await waitAfterNextRender(helpBubble);

    const elements = getProgressIndicators();
    assertEquals(2, elements.length, 'there should be two elements');
    assertTrue(
        elements.item(0)!.classList.contains('total-progress'),
        'element 0 should have total-progress class');
    assertTrue(
        elements.item(1)!.classList.contains('total-progress'),
        'element 1 should have total-progress class');
  });
});
