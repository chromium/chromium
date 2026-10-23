// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {AppMenuIconType, AppMenuSeverity, IconTable, IconType} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

suite('IPH', function() {
  // Distinct fake colors so that the normal and IPH colors can be told apart.
  const ICON_COLOR = 'rgb(4, 5, 6)';
  const PROMO_COLOR = 'rgb(1, 2, 3)';
  const RIPPLE_COLOR = 'rgb(7, 8, 9)';
  const APP_MENU_RIPPLE_COLOR = 'rgb(10, 11, 12)';
  const ATTENTION_COLOR = 'rgb(13, 14, 15)';

  const FAKE_COLORS: Record<string, string> = {
    '--color-toolbar-button-icon': ICON_COLOR,
    '--color-toolbar-feature-promo-highlight': PROMO_COLOR,
    '--color-toolbar-ink-drop-ripple': RIPPLE_COLOR,
    '--color-app-menu-chip-ink-drop-ripple': APP_MENU_RIPPLE_COLOR,
    '--color-button-feature-attention-highlight': ATTENTION_COLOR,
  };

  setup(() => {
    for (const [name, value] of Object.entries(FAKE_COLORS)) {
      document.documentElement.style.setProperty(name, value);
    }
  });

  teardown(() => {
    for (const name of Object.keys(FAKE_COLORS)) {
      document.documentElement.style.removeProperty(name);
    }
  });

  function getIconColor(visualTarget: HTMLElement): string {
    if (visualTarget.tagName === 'CR-ICON-BUTTON') {
      return window.getComputedStyle(visualTarget)
          .getPropertyValue('--cr-icon-button-fill-color')
          .trim();
    }
    // toolbar-chip-button icons use currentColor.
    return window.getComputedStyle(visualTarget).color;
  }

  /**
   * @param expectIconRecolor Whether the icon is expected to be painted in the
   *     feature promo highlight color during the IPH, matching native
   *     ToolbarButton::GetForegroundColor() and its overrides.
   * @param expectedPressedColor The pressed (ripple) color expected on top of
   *     the attention color in the pulse, matching the native ink drop ripple.
   */
  async function testIPH(
      tagName: string, setNormalState: (el: any) => void,
      expectIconRecolor: boolean = true,
      expectedPressedColor: string = RIPPLE_COLOR) {
    const el = document.createElement(tagName) as any;
    setNormalState(el);
    document.body.appendChild(el);
    await microtasksFinished();

    let button = el.shadowRoot!.querySelector(
        'cr-icon-button, cr-button, toolbar-chip-button')!;
    if (button.tagName === 'TOOLBAR-CHIP-BUTTON') {
      button = button.shadowRoot!.querySelector('#button')!;
    }
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

    const hostWithHighlight =
        el.shadowRoot!.querySelector('toolbar-chip-button') || el;
    const visualTarget = el.shadowRoot!.querySelector('.iph-visual-target') ||
        el.shadowRoot!.querySelector('toolbar-chip-button')
            ?.shadowRoot!.querySelector('.iph-visual-target')!;
    assertTrue(
        !!visualTarget, `Button ${tagName} should have an iph-visual-target`);

    assertEquals(
        'none', window.getComputedStyle(visualTarget, '::before').animationName,
        `Button ${tagName} should not pulse when highlight is inactive`);
    assertNotEquals(
        PROMO_COLOR, getIconColor(visualTarget),
        `Button ${tagName} icon should not use the IPH color without an IPH`);

    hostWithHighlight.classList.add('help-anchor-highlight');
    await microtasksFinished();

    assertEquals(
        'pulse',
        window.getComputedStyle(visualTarget, '::before').animationName,
        `Button ${tagName} should pulse when help-anchor-highlight is active`);
    assertEquals(
        '1', window.getComputedStyle(visualTarget, '::before').opacity,
        `Button ${tagName} pulse layer should have opacity: 1`);
    assertEquals(
        ATTENTION_COLOR,
        window.getComputedStyle(visualTarget, '::before').backgroundColor,
        `Button ${tagName} pulse should paint the attention color underneath`);
    assertEquals(
        `linear-gradient(${expectedPressedColor})`,
        window.getComputedStyle(visualTarget, '::before').backgroundImage,
        `Button ${tagName} pulse should paint the pressed color on top`);
    if (expectIconRecolor) {
      assertEquals(
          PROMO_COLOR, getIconColor(visualTarget),
          `Button ${tagName} icon should use the IPH color during the IPH`);
    } else {
      assertNotEquals(
          PROMO_COLOR, getIconColor(visualTarget),
          `Button ${tagName} icon should keep its color during the IPH`);
    }

    hostWithHighlight.classList.remove('help-anchor-highlight');
    await microtasksFinished();

    assertNotEquals(
        PROMO_COLOR, getIconColor(visualTarget),
        `Button ${tagName} icon should restore its color after the IPH`);

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
    // The avatar icon comes from the browser through the icon table, so CSS
    // must not change it. Native only recolors the incognito vector icon for
    // the IPH; profile pictures ignore the IPH color.
    await testIPH('avatar-button', (el) => {
      el.state = {
        isVisible: true,
        accessibilityDescription: 'Avatar',
        tooltip: 'Avatar',
        icon: {handleId: 0n},
      };
    }, /*expectIconRecolor=*/ false);
  });

  test('SplitTabsButton', async () => {
    await testIPH('split-tabs-button', (el) => {
      el.state = {isCurrentTabSplit: false};
    });
  });

  test('SplitTabsButtonEngaged', async () => {
    // Native SplitTabsToolbarButton keeps the engaged color during the IPH.
    await testIPH('split-tabs-button', (el) => {
      el.state = {isCurrentTabSplit: true};
    }, /*expectIconRecolor=*/ false);
  });

  test('PinnedToolbarAction', async () => {
    // Register an icon with an explicit color, so that the pinned toolbar
    // action sets the icon color inline (see
    // PinnedToolbarActionElement.getIconStyle_()).
    IconTable.getInstance().applyUpdates([{
      handleId: 1n,
      iconUrlOrName: 'cr:schedule',
      iconType: IconType.kIconSet,
      color: {value: 0xFF123456},
    }]);

    await testIPH('pinned-toolbar-action', (el) => {
      el.state = {
        action: 1,
        enabled: true,
        tooltip: 'Pinned Action',
        icon: {handleId: 1n},
      };
    });
  });

  test('PinnedToolbarActionEngaged', async () => {
    // Native PinnedActionToolbarButton keeps the engaged color during the IPH.
    await testIPH('pinned-toolbar-action', (el) => {
      el.state = {
        action: 1,
        enabled: true,
        activated: true,
        tooltip: 'Pinned Action',
        icon: {handleId: 0n},
      };
    }, /*expectIconRecolor=*/ false);
  });

  test('AppMenuButton', async () => {
    await testIPH('app-menu-button', (el) => {
      el.state = {
        iconType: AppMenuIconType.kNone,
        severity: AppMenuSeverity.kNone,
        labelText: null,
        accessibilityText: '',
        tooltip: 'App Menu',
        isContextMenuVisible: false,
        windowIsMaximizedOrFullscreen: false,
      };
    });
  });

  test('AppMenuButtonWithLabel', async () => {
    // Native BrowserAppMenuButton keeps the label colors while the label is
    // visible, and uses the app menu chip ripple.
    await testIPH('app-menu-button', (el) => {
      el.state = {
        iconType: AppMenuIconType.kNone,
        severity: AppMenuSeverity.kNone,
        labelText: 'Update',
        accessibilityText: '',
        tooltip: 'App Menu',
        isContextMenuVisible: false,
        windowIsMaximizedOrFullscreen: false,
      };
    }, /*expectIconRecolor=*/ false, APP_MENU_RIPPLE_COLOR);
  });
});
