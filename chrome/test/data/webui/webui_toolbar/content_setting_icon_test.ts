// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, ContentSettingImageType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ContentSettingIconElement} from 'chrome://webui-toolbar.top-chrome/app.js';

class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super([
      'showContentSettingsBubble',
      'onContentSettingImageAnimationEnded',
    ]);
  }

  showContentSettingsBubble(type: ContentSettingImageType) {
    this.methodCalled('showContentSettingsBubble', type);
  }

  onContentSettingImageAnimationEnded(type: ContentSettingImageType) {
    this.methodCalled('onContentSettingImageAnimationEnded', type);
  }
}

suite('ContentSettingIcon', function() {
  let icon: ContentSettingIconElement;
  let handler: TestToolbarUiHandler;

  setup(async () => {
    handler = new TestToolbarUiHandler();
    BrowserProxyImpl.setInstance({toolbarUIHandler: handler} as any);

    const trustedTypes = window.trustedTypes!;
    document.body.innerHTML = trustedTypes.emptyHTML;
    icon = document.createElement('content-setting-icon');
    icon.state = {
      type: ContentSettingImageType.kCookies,
      isBlocked: false,
      tooltip: 'Tooltip',
      accessibilityString: 'Accessible Name',
      isBubbleVisible: false,
      shouldRunAnimation: false,
      explanatoryString: '',
    };
    document.body.appendChild(icon);
    await microtasksFinished();
  });

  test('ARIA label', () => {
    const innerButton = icon.$.chip.$.button;
    assertTrue(!!innerButton);
    assertEquals('Accessible Name', innerButton.getAttribute('aria-label'));
  });

  test('Animation', async () => {
    assertFalse(icon.hasAttribute('should-run-animation'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: 'Blocked',
    };
    await microtasksFinished();
    assertTrue(icon.hasAttribute('should-run-animation'));
    assertEquals(1, icon.$.label.getAnimations().length);
    assertEquals('Blocked', icon.$.label.textContent.trim());

    // Trigger animationend
    icon.$.label.dispatchEvent(new Event('animationend'));
    await microtasksFinished();
    const type =
        await handler.whenCalled('onContentSettingImageAnimationEnded');
    assertEquals(ContentSettingImageType.kCookies, type);
  });

  test('SpuriousUpdateDuringAnimation', async () => {
    assertFalse(icon.hasAttribute('should-run-animation'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: 'Blocked',
    };
    await microtasksFinished();
    assertTrue(icon.hasAttribute('should-run-animation'));
    assertEquals(1, icon.$.label.getAnimations().length);

    // Perform a spurious state update with a new object reference.
    icon.state = {
      ...icon.state,
    };
    await microtasksFinished();
    // Spurious update should NOT cancel the in-progress CSS animation.
    assertTrue(icon.hasAttribute('should-run-animation'));
    assertEquals(1, icon.$.label.getAnimations().length);
  });

  test('NoAnimationWithoutExplanatoryString', async () => {
    assertFalse(icon.hasAttribute('should-run-animation'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: '',
    };
    await microtasksFinished();
    assertTrue(icon.hasAttribute('should-run-animation'));
    assertEquals(0, icon.$.label.getAnimations().length);
  });

  test('AnimationWithMultipleIcons', async () => {
    const container = document.createElement('content-settings-icons');
    document.body.appendChild(container);

    const cookiesState = {
      type: ContentSettingImageType.kCookies,
      isBlocked: true,
      tooltip: 'Cookies',
      accessibilityString: 'Cookies',
      isBubbleVisible: false,
      shouldRunAnimation: false,
      explanatoryString: '',
    };
    const popupsState = {
      type: ContentSettingImageType.kPopups,
      isBlocked: true,
      tooltip: 'Popups',
      accessibilityString: 'Popups',
      isBubbleVisible: false,
      shouldRunAnimation: true,
      explanatoryString: 'Popups blocked',
    };

    // Use order [Popups, Cookies] to test element reuse if Popups is removed.
    container.contentSettingImageStates = [popupsState, cookiesState];
    await microtasksFinished();

    let icons = container.shadowRoot.querySelectorAll('content-setting-icon');
    assertEquals(2, icons.length);
    assertTrue(icons[0]!.hasAttribute('should-run-animation'));
    assertEquals(1, icons[0]!.$.label.getAnimations().length);
    assertFalse(icons[1]!.hasAttribute('should-run-animation'));
    assertEquals(0, icons[1]!.$.label.getAnimations().length);

    // Immediately remove the popups icon.
    container.contentSettingImageStates = [cookiesState];
    await microtasksFinished();

    icons = container.shadowRoot.querySelectorAll('content-setting-icon');
    assertEquals(1, icons.length);
    assertEquals(ContentSettingImageType.kCookies, icons[0]!.state.type);
    assertFalse(icons[0]!.hasAttribute('should-run-animation'));
    assertEquals(0, icons[0]!.$.label.getAnimations().length);
  });

  test('RightClick', () => {
    const button = icon.$.chip;

    // contextmenu should prevent default and NOT open the bubble.
    const contextMenuEvent =
        new PointerEvent('contextmenu', {cancelable: true});
    button.dispatchEvent(contextMenuEvent);
    assertTrue(contextMenuEvent.defaultPrevented);
    assertEquals(0, handler.getCallCount('showContentSettingsBubble'));

    // auxclick should open the bubble.
    button.dispatchEvent(new PointerEvent('auxclick', {button: 2}));
    assertEquals(1, handler.getCallCount('showContentSettingsBubble'));
  });
});
