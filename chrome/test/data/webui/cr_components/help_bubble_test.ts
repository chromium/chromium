// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://resources/cr_components/help_bubble/help_bubble.js';

import {HelpBubbleElement} from 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import {HelpBubblePosition} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    assertFalse(helpBubble.open);
    assertEquals(null, helpBubble.getAnchorElement());
  });

  test('help bubble shows and anchors correctly', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.body = 'help bubble body';
    helpBubble.show();

    assertTrue(helpBubble.open);
    assertEquals(
        document.querySelector<HTMLElement>('#p1'),
        helpBubble.getAnchorElement());
    assertEquals(
        'help bubble body',
        helpBubble.shadowRoot!.querySelector<HTMLElement>(
                                  'div.body')!.textContent);
  });

  test('help bubble closes', () => {
    helpBubble.anchorId = 'title';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.body = 'help bubble body';
    helpBubble.show();

    assertTrue(helpBubble.open);
    assertEquals(
        document.querySelector<HTMLElement>('#title'),
        helpBubble.getAnchorElement());

    helpBubble.hide();
    assertFalse(helpBubble.open);
    assertEquals(null, helpBubble.getAnchorElement());
  });
});
