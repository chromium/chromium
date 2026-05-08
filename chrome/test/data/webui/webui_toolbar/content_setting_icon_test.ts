// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {ContentSettingImageType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ContentSettingIconElement} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('ContentSettingIcon', function() {
  let icon: ContentSettingIconElement;

  setup(async () => {
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
    assertEquals('Accessible Name', icon.$.button.getAttribute('aria-label'));
  });

  test('Animation', async () => {
    assertFalse(icon.hasAttribute('animating'));
    icon.state = {
      ...icon.state,
      shouldRunAnimation: true,
      explanatoryString: 'Blocked',
    };
    await microtasksFinished();
    assertTrue(icon.hasAttribute('animating'));

    assertEquals('Blocked', icon.$.label.textContent.trim());

    // Trigger animationend
    icon.$.label.dispatchEvent(new Event('animationend'));
    await microtasksFinished();
    assertFalse(icon.hasAttribute('animating'));
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
    assertTrue(icons[0]!.hasAttribute('animating'));
    assertFalse(icons[1]!.hasAttribute('animating'));

    // Immediately remove the popups icon.
    container.contentSettingImageStates = [cookiesState];
    await microtasksFinished();

    icons = container.shadowRoot.querySelectorAll('content-setting-icon');
    assertEquals(1, icons.length);
    assertEquals(ContentSettingImageType.kCookies, icons[0]!.state.type);
    assertFalse(icons[0]!.hasAttribute('animating'));
  });
});
