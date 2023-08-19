// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleHeaderElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {assertStyle, capture, render} from '../test_support.js';

suite('NewTabPageModulesModuleHeaderTest', () => {
  let moduleHeader: ModuleHeaderElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    moduleHeader = new ModuleHeaderElement();
    document.body.appendChild(moduleHeader);
    render(moduleHeader);
  });

  test('setting text shows text', () => {
    // Act.
    moduleHeader.chipText = 'foo';
    moduleHeader.descriptionText = 'bar';
    moduleHeader.showDismissButton = true;
    moduleHeader.dismissText = 'baz';
    moduleHeader.disableText = 'abc';
    moduleHeader.moreActionsText = 'def';
    moduleHeader.showInfoButtonDropdown = true;
    render(moduleHeader);

    // Assert.
    assertEquals(
        'foo', $$<HTMLElement>(moduleHeader, '#chip')!.textContent!.trim());
    assertEquals(
        'bar',
        $$<HTMLElement>(moduleHeader, '#description')!.textContent!.trim());
    assertEquals(
        'baz',
        $$<HTMLElement>(moduleHeader, '#dismissButton')!.textContent!.trim());
    assertEquals(
        'abc',
        $$<HTMLElement>(moduleHeader, '#disableButton')!.textContent!.trim());
    assertEquals(
        'def', $$<HTMLElement>(moduleHeader, '#menuButton')!.title!.trim());
    assertEquals(
        'About this card',
        $$<HTMLElement>(moduleHeader, '#infoButton')!.textContent!.trim());
  });

  test('clicking buttons sends events', () => {
    // Arrange.
    const infoButtonClick = capture(moduleHeader, 'info-button-click');
    const dismissButtonClick = capture(moduleHeader, 'dismiss-button-click');
    const disableButtonClick = capture(moduleHeader, 'disable-button-click');
    const customizeModule = capture(moduleHeader, 'customize-module');
    moduleHeader.showInfoButton = true;
    moduleHeader.showDismissButton = true;
    render(moduleHeader);

    // Act.
    $$<HTMLElement>(moduleHeader, '#infoButton')!.click();
    $$<HTMLElement>(moduleHeader, '#dismissButton')!.click();
    $$<HTMLElement>(moduleHeader, '#disableButton')!.click();
    $$<HTMLElement>(moduleHeader, '#customizeButton')!.click();

    // Assert.
    assertTrue(infoButtonClick.received);
    assertTrue(dismissButtonClick.received);
    assertTrue(disableButtonClick.received);
    assertTrue(customizeModule.received);
  });

  test('action menu opens and closes', () => {
    // Act & Assert.
    assertFalse(moduleHeader.$.actionMenu.open);
    $$<HTMLElement>(moduleHeader, '#menuButton')!.click();
    assertTrue(moduleHeader.$.actionMenu.open);
    $$<HTMLElement>(moduleHeader, '#disableButton')!.click();
    assertFalse(moduleHeader.$.actionMenu.open);
  });

  test('can hide menu button', () => {
    // Act.
    moduleHeader.hideMenuButton = true;
    render(moduleHeader);

    // Assert.
    assertFalse(!!$$(moduleHeader, '#menuButton'));
  });

  suite('module header icon', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({modulesHeaderIconEnabled: true});
    });

    test('icon appears', () => {
      // Act.
      moduleHeader.iconSrc = 'icons/module_logo.svg';
      render(moduleHeader);

      // Assert.
      assertStyle(
          $$<HTMLImageElement>(moduleHeader, '.module-icon')!,
          '-webkit-mask-image',
          'url("chrome://new-tab-page/icons/module_logo.svg")');
    });
  });
});
