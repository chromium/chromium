// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('IPH', function() {
  async function testIPH(tagName: string, setNormalState: (el: any) => void) {
    const el = document.createElement(tagName) as any;
    setNormalState(el);
    document.body.appendChild(el);
    await microtasksFinished();

    const button = el.shadowRoot!.querySelector('cr-icon-button, cr-button')!;
    const normalTooltip = button.getAttribute('title') || '';
    assertTrue(
        normalTooltip.length > 0,
        `Button ${tagName} should have a tooltip initially`);
    el.hasHelpBubble = true;
    await microtasksFinished();

    assertEquals(
        '', button.getAttribute('title'),
        `Button ${tagName} should have no tooltip during IPH`);

    el.hasHelpBubble = false;
    await microtasksFinished();

    assertEquals(
        normalTooltip, button.getAttribute('title'),
        `Button ${tagName} should restore tooltip after IPH`);

    el.remove();
  }

  test('BackForwardButton', async () => {
    await testIPH('back-forward-button', (el) => {
      el.direction = 'back';
      el.state = {enabled: true, shouldBeShown: true};
    });
  });

  test('ReloadButton', async () => {
    await testIPH('reload-button', (el) => {
      el.state = {
        canShowMenu: false,
        isNavigationLoading: false,
        isContextMenuVisible: false,
        doubleClickInterval: {microseconds: 500000n},
      };
    });
  });

  test('HomeButton', async () => {
    await testIPH('home-button', (el) => {
      el.state = {shouldBeShown: true, isContextMenuVisible: false};
    });
  });

  test('AvatarButton', async () => {
    await testIPH('avatar-button', (el) => {
      el.state = {
        isVisible: true,
        accessibilityDescription: 'Avatar',
        tooltip: 'Avatar',
      };
    });
  });

  test('SplitTabsButton', async () => {
    await testIPH('split-tabs-button', (el) => {
      el.state = {isCurrentTabSplit: false, isPinned: true};
    });
  });

  test('PinnedToolbarAction', async () => {
    await testIPH('pinned-toolbar-action', (el) => {
      el.state = {
        action: 1,
        enabled: true,
        tooltip: 'Pinned Action',
        icon: {handleId: 0n},
      };
    });
  });
});
