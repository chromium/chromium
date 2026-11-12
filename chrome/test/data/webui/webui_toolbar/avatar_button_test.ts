// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';
import 'chrome://webui-toolbar.top-chrome/lazy_load.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import type {AvatarButtonElement} from 'chrome://webui-toolbar.top-chrome/lazy_load.js';
import {AvatarToolbarButtonState, BrowserProxyImpl} from 'chrome://webui-toolbar.top-chrome/webui_toolbar.js';

import {TestToolbarBrowserProxy} from './test_toolbar_browser_proxy.js';

// Height of the button, and also the width of the collapsed button.
const BUTTON_HEIGHT = 34;

// A point at which the button's display mode changes. See getDisplayMode().
interface Transition {
  // The narrowest width at which `mode` is used.
  width: number;
  mode: string;
  // The mode used with one less pixel of width.
  narrowerMode: string;
}

suite('AvatarButtonTest', function() {
  let avatar: AvatarButtonElement;
  let testBrowserProxy: TestToolbarBrowserProxy;
  // Width of the fake toolbar's window, used to compute the available width.
  let windowWidth: number;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestToolbarBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // A fake toolbar containing only the avatar button, which provides the
    // methods the button uses during layout. Layout constants are normally
    // provided by the toolbar, so set the ones that affect the button's size.
    const toolbar = document.createElement('div');
    toolbar.style.display = 'flex';
    toolbar.style.setProperty('--toolbar-button-height', `${BUTTON_HEIGHT}px`);
    toolbar.style.setProperty('--toolbar-button-icon-size', '20px');
    toolbar.style.setProperty('--avatar-chip-padding-left', '10px');
    toolbar.style.setProperty('--avatar-chip-padding-right', '4px');
    toolbar.style.setProperty('--avatar-chip-icon-label-spacing', '6px');
    // Remove the inner button's outer margins, which are the difference
    // between these, to keep the widths below simple.
    toolbar.style.setProperty(
        '--toolbar-button-refresh-expanded-margin', '0px');
    toolbar.style.setProperty('--toolbar-icon-default-margin', '0px');
    const getAvatarWidth = () => avatar.getBoundingClientRect().width;
    Object.assign(toolbar, {
      getAvailableWidth: () => windowWidth - Math.round(getAvatarWidth()),
      getExactAvailableWidth: () => windowWidth - getAvatarWidth(),
    });
    document.body.appendChild(toolbar);

    // Unlike in the real toolbar, nothing removes the button's `initial-load`
    // class, so the label's width transitions stay disabled.
    avatar = document.createElement('avatar-button');
    toolbar.attachShadow({mode: 'open'}).appendChild(avatar);
    await setState({text: 'Work Profile Name'});
  });

  async function setState(fields: Partial<AvatarButtonElement['state']>) {
    avatar.state = {...avatar.state, ...fields};
    await avatar.updateComplete;
    await microtasksFinished();
  }

  // Lays out the button as the toolbar would, with `width` pixels available to
  // it.
  function layout(width: number) {
    windowWidth = width;
    avatar.setToMinWidth();
    avatar.expandUpToPreferredWidth();
  }

  function isHidden(): boolean {
    return avatar.classList.contains('overflow-display-none');
  }

  // Returns how the button is currently displayed: 'hidden' if the button is
  // completely hidden due to overflow, 'full' if the full label is being
  // displayed with no ellipsis (this is a subset of the 'text' label mode), or
  // its label mode, if neither of those is the case.
  function getDisplayMode(): string {
    if (isHidden()) {
      return 'hidden';
    }
    const mode = avatar.getAttribute('label-mode')!;
    return mode === 'text' && !avatar.$.button.style.width ? 'full' : mode;
  }

  // Returns the width of the first `length` UTF-16 code units of the label.
  function getLabelPrefixWidth(length: number): number {
    const range = document.createRange();
    range.setStart(avatar.$.text.firstChild!, 0);
    range.setEnd(avatar.$.text.firstChild!, length);
    return range.getBoundingClientRect().width;
  }

  test('LabelModeDependsOnAvailableWidth', function() {
    // Measure the button with its full label displayed.
    avatar.setToPreferredWidth();
    const fullWidth = avatar.getBoundingClientRect().width;
    const textWidth = avatar.$.text.getBoundingClientRect().width;
    const ellipsisWidth = avatar.$.ellipsis.getBoundingClientRect().width;
    const firstCharacterWidth = getLabelPrefixWidth(1);
    // The width with an empty label area.
    const noLabelWidth = fullWidth - textWidth;

    // The label area gets all available width beyond `noLabelWidth`, so these
    // widths are exact.
    const transitions: Transition[] = [
      {
        // The fake toolbar rounds widths, like the real one.
        width: Math.round(fullWidth),
        mode: 'full',
        narrowerMode: 'text',
      },
      {
        width: Math.ceil(noLabelWidth + ellipsisWidth + firstCharacterWidth),
        mode: 'text',
        narrowerMode: 'ellipsis',
      },
      {
        width: Math.ceil(noLabelWidth + ellipsisWidth),
        mode: 'ellipsis',
        narrowerMode: 'space-only',
      },
      {
        width: Math.ceil(noLabelWidth),
        mode: 'space-only',
        narrowerMode: 'collapsed',
      },
      {
        width: BUTTON_HEIGHT,
        mode: 'collapsed',
        narrowerMode: 'hidden',
      },
    ];
    for (const {width, mode, narrowerMode} of transitions) {
      layout(width);
      assertEquals(mode, getDisplayMode(), `At width ${width}`);
      layout(width - 1);
      assertEquals(narrowerMode, getDisplayMode(), `At width ${width - 1}`);
    }

    // When collapsed, the button is styled as if it had no label area.
    layout(BUTTON_HEIGHT);
    assertFalse(avatar.$.button.hasAttribute('has-label'));
  });

  // The 'text' label mode should only be used when there's room for the
  // entire first grapheme (user-perceived character) of the label, followed by
  // an ellipsis, even when that grapheme consists of multiple UTF-16 code
  // units.
  test('EllipsisModeWithMultiCodeUnitFirstGrapheme', async function() {
    // `graphemeLength` is the length of the label's first grapheme, in UTF-16
    // code units.
    const labels = [
      // 'e' followed by U+0301 COMBINING ACUTE ACCENT.
      {text: 'e\u0301clair', graphemeLength: 2},
      // U+1D400 MATHEMATICAL BOLD CAPITAL A, a surrogate pair.
      {text: '\u{1D400}lpha', graphemeLength: 2},
      // A flag emoji: two regional indicator symbols, each a surrogate pair.
      {text: '\u{1F1FA}\u{1F1F8} Profile', graphemeLength: 4},
      // An emoji ZWJ sequence: U+1F469 WOMAN, ZERO WIDTH JOINER, U+1F4BB
      // PERSONAL COMPUTER.
      {text: '\u{1F469}\u200D\u{1F4BB} Work', graphemeLength: 5},
    ];
    for (const {text, graphemeLength} of labels) {
      await setState({text});

      avatar.setToPreferredWidth();
      const fullWidth = avatar.getBoundingClientRect().width;
      const textWidth = avatar.$.text.getBoundingClientRect().width;
      const ellipsisWidth = avatar.$.ellipsis.getBoundingClientRect().width;
      const firstGraphemeWidth = getLabelPrefixWidth(graphemeLength);
      const noLabelWidth = fullWidth - textWidth;

      const width =
          Math.ceil(noLabelWidth + ellipsisWidth + firstGraphemeWidth);
      layout(width);
      assertEquals('text', getDisplayMode(), `"${text}" at width ${width}`);
      layout(width - 1);
      assertEquals(
          'ellipsis', getDisplayMode(), `"${text}" at width ${width - 1}`);
    }
  });

  // When the label is a single grapheme, there's never room to display part of
  // the label followed by an ellipsis, so the button should switch directly
  // from displaying the full label to displaying only an ellipsis.
  test('SingleMultiCodeUnitGraphemeLabel', async function() {
    // Make the label's font much larger than the ellipsis's, so the label is
    // much wider than the ellipsis. Otherwise, it's possible than an ellipsis
    // would be larger than the grapheme, and we would go from full to
    // space-only instead, which is an entirely valid transition, we just want
    // this test to behave consistently, so simplest to avoid that case.
    avatar.$.text.style.fontSize = '48px';

    const labels = [
      // A flag emoji: two regional indicator symbols, each a surrogate pair.
      '\u{1F1FA}\u{1F1F8}',
      // An emoji ZWJ sequence: U+1F469 WOMAN, ZERO WIDTH JOINER, U+1F4BB
      // PERSONAL COMPUTER.
      '\u{1F469}\u200D\u{1F4BB}',
    ];
    for (const text of labels) {
      await setState({text});

      avatar.setToPreferredWidth();
      const fullWidth = avatar.getBoundingClientRect().width;
      const textWidth = avatar.$.text.getBoundingClientRect().width;
      const ellipsisWidth = avatar.$.ellipsis.getBoundingClientRect().width;
      // Make sure the ellipsis is narrow enough to fit in place of the label
      // after the button is shrunk by one pixel, with an extra pixel to avoid
      // rounding issues.
      assertTrue(
          textWidth - 2 >= ellipsisWidth,
          `"${text}" is ${textWidth}px wide, ellipsis is ${ellipsisWidth}px`);

      const width = Math.round(fullWidth);
      layout(width);
      assertEquals('full', getDisplayMode(), `"${text}" at width ${width}`);
      layout(width - 1);
      assertEquals(
          'ellipsis', getDisplayMode(), `"${text}" at width ${width - 1}`);
    }
  });

  test('SetToPreferredWidthRestoresFullLabel', function() {
    layout(0);
    assertTrue(isHidden());
    assertEquals('collapsed', avatar.getAttribute('label-mode'));

    avatar.setToPreferredWidth();
    assertFalse(isHidden());
    assertEquals('text', avatar.getAttribute('label-mode'));
    assertEquals('', avatar.$.button.style.width);
    assertTrue(avatar.$.button.hasAttribute('has-label'));
  });

  test('PreventOverflowKeepsCollapsedButton', async function() {
    await setState({preventOverflow: true});

    // With no space, the button stays visible, collapsed to its icon.
    layout(0);
    assertFalse(isHidden());
    assertEquals('collapsed', avatar.getAttribute('label-mode'));
    assertEquals(BUTTON_HEIGHT, avatar.getBoundingClientRect().width);
  });

  test('LabelModeTracksText', async function() {
    const chip = avatar.$.button;
    assertEquals('text', avatar.getAttribute('label-mode'));
    assertTrue(chip.hasAttribute('has-label'));
    assertTrue(chip.hasAttribute('outset-focus-ring'));

    await setState({text: ''});
    assertEquals('no-text', avatar.getAttribute('label-mode'));
    assertFalse(chip.hasAttribute('has-label'));
    assertFalse(chip.hasAttribute('outset-focus-ring'));

    await setState({text: 'Work'});
    assertEquals('text', avatar.getAttribute('label-mode'));
    assertTrue(chip.hasAttribute('has-label'));
    assertTrue(chip.hasAttribute('outset-focus-ring'));

    // When collapsed, the button keeps its outset focus ring, since its
    // highlight is still displayed.
    layout(0);
    assertEquals('collapsed', avatar.getAttribute('label-mode'));
    assertTrue(chip.hasAttribute('outset-focus-ring'));
  });

  test('RequestLayout', async function() {
    let requestLayoutCount = 0;
    avatar.addEventListener('request-layout', () => ++requestLayoutCount);

    await setState({tooltip: 'New tooltip'});
    assertEquals(0, requestLayoutCount);

    await setState({text: 'Other name'});
    assertEquals(1, requestLayoutCount);

    await setState({state: AvatarToolbarButtonState.kGuestSession});
    assertEquals(2, requestLayoutCount);

    await setState({hasLinearGradientRing: true});
    assertEquals(3, requestLayoutCount);
  });

  test('ClickDisabled', async function() {
    assertTrue(avatar.state.enabled);

    await setState({enabled: false});
    assertFalse(avatar.state.enabled);

    // Clicking the disabled button should not call showAvatarMenu.
    avatar.$.button.click();
    assertEquals(
        0, testBrowserProxy.toolbarUIHandler.getCallCount('showAvatarMenu'));

    // Re-enabling and clicking should call showAvatarMenu.
    await setState({enabled: true});
    assertTrue(avatar.state.enabled);

    avatar.$.button.click();
    assertEquals(
        1, testBrowserProxy.toolbarUIHandler.getCallCount('showAvatarMenu'));
  });
});
