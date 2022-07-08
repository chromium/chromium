// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://resources/cr_components/help_bubble/help_bubble.js';

import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.m.js';
import {HELP_BUBBLE_DISMISSED_EVENT, HelpBubbleDismissedEvent, HelpBubbleElement} from 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import {HelpBubblePosition} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('CrComponentsHelpBubbleTest', () => {
  let helpBubble: HelpBubbleElement;

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
    assertEquals(null, helpBubble.getAnchorElement());
  });

  const HELP_BUBBLE_BODY = 'help bubble body';
  const HELP_BUBBLE_TITLE = 'help bubble title';

  test('help bubble shows and anchors correctly', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();

    assertEquals(
        document.querySelector<HTMLElement>('#p1'),
        helpBubble.getAnchorElement());
    assertEquals(HELP_BUBBLE_BODY, helpBubble.$.body.textContent);
    assertTrue(isVisible(helpBubble));
  });

  test('help bubble titles shows', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.titleText = HELP_BUBBLE_TITLE;
    helpBubble.show();

    assertTrue(isVisible(helpBubble));
    const titleElement = helpBubble.$.title;
    assertEquals(HELP_BUBBLE_TITLE, titleElement.textContent);
    assertTrue(isVisible(titleElement));
  });

  test('help bubble titles hides when no title set', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();

    assertTrue(isVisible(helpBubble));
    const titleElement = helpBubble.$.title;
    assertFalse(isVisible(titleElement));
  });

  test('help bubble closes', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();

    assertEquals(
        document.querySelector<HTMLElement>('#title'),
        helpBubble.getAnchorElement());

    helpBubble.hide();
    assertEquals(null, helpBubble.getAnchorElement());
    assertFalse(isVisible(helpBubble));
  });

  test('help bubble open close open', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();
    helpBubble.hide();
    helpBubble.show();
    assertEquals(
        document.querySelector<HTMLElement>('#title'),
        helpBubble.getAnchorElement());
    assertEquals(HELP_BUBBLE_BODY, helpBubble.$.body.textContent);
    assertTrue(isVisible(helpBubble));
  });

  test('help bubble close button has correct alt text', () => {
    const CLOSE_TEXT: string = 'Close button text.';
    helpBubble.closeText = CLOSE_TEXT;
    assertEquals(CLOSE_TEXT, helpBubble.$.close.getAttribute('aria-label'));
  });

  test('help bubble click close button generates event', () => {
    let clicked: number = 0;
    const callback = (e: HelpBubbleDismissedEvent) => {
      assertEquals('title', e.detail.anchorId);
      assertFalse(e.detail.fromActionButton);
      ++clicked;
    };
    helpBubble.addEventListener(HELP_BUBBLE_DISMISSED_EVENT, callback);
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.show();
    const closeButton = helpBubble.$.close;
    assertEquals(0, clicked);
    closeButton.click();
    assertEquals(1, clicked);
  });

  test('help bubble adds one button', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = ['button1'];
    helpBubble.show();
    assertEquals(1, helpBubble.$.buttons.children.length);
    const button = helpBubble.getButtonForTesting(0);
    assertTrue(!!button);
    assertEquals(helpBubble.buttons[0], button.textContent);
    assertFalse(button.classList.contains('default-button'));
  });

  test('help bubble adds several buttons', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = ['button1', 'button2', 'button3'];
    helpBubble.show();
    assertEquals(3, helpBubble.$.buttons.children.length);
    for (let i: number = 0; i < 3; ++i) {
      const button = helpBubble.getButtonForTesting(i);
      assertTrue(!!button);
      assertEquals(helpBubble.buttons[i], button.textContent);
      assertFalse(button.classList.contains('default-button'));
    }
  });

  test('help bubble adds default button', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = ['button1'];
    helpBubble.defaultButtonIndex = 0;
    helpBubble.show();
    const button = helpBubble.getButtonForTesting(0);
    assertTrue(!!button);
    assertTrue(button.classList.contains('default-button'));
  });

  test('help bubble adds default button among several', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = ['button1', 'button2', 'button3'];
    helpBubble.defaultButtonIndex = 1;
    helpBubble.show();
    assertEquals(3, helpBubble.$.buttons.children.length);

    // Make sure all buttons were created as expected, including the default
    // button.
    let defaultButton: CrButtonElement|null = null;
    for (let i: number = 0; i < 3; ++i) {
      const button = helpBubble.getButtonForTesting(i);
      assertTrue(!!button);
      assertEquals(helpBubble.buttons[i], button.textContent);
      const isDefault = (i === helpBubble.defaultButtonIndex);
      assertEquals(isDefault, button.classList.contains('default-button'));
      if (isDefault) {
        defaultButton = button;
      }
    }

    // Verify that the default button is in the expected position.
    assertTrue(!!defaultButton);
    const expectedIndex = HelpBubbleElement.isDefaultButtonLeading() ? 0 : 2;
    assertEquals(
        defaultButton, helpBubble.$.buttons.children.item(expectedIndex));
  });

  test('help bubble click action button generates event', () => {
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
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.bodyText = HELP_BUBBLE_BODY;
    helpBubble.buttons = ['button1', 'button2', 'button3'];
    helpBubble.defaultButtonIndex = 1;

    for (let i: number = 0; i < 3; ++i) {
      clicked = false;
      buttonIndex = -1;
      helpBubble.show();
      const button = helpBubble.getButtonForTesting(i);
      assertTrue(!!button);
      button.click();
      assertTrue(clicked);
      assertEquals(i, buttonIndex);
      helpBubble.hide();
    }
  });
});
