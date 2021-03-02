// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, ModuleHeaderElement} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

/** @param {!HTMLElement} element */
function render(element) {
  element.shadowRoot.querySelectorAll('dom-if').forEach(tmpl => tmpl.render());
}

/**
 * @param {!HTMLElement} target
 * @param {string} event
 * @return {{received: boolean}}
 */
function capture(target, event) {
  const capture = {received: false};
  target.addEventListener(event, () => capture.received = true);
  return capture;
}

suite('NewTabPageModulesModuleHeaderTest', () => {
  /** @type {!ModuleHeaderElement} */
  let moduleHeader;

  setup(() => {
    document.body.innerHTML = '';
    moduleHeader = new ModuleHeaderElement();
    document.body.appendChild(moduleHeader);
  });

  test('setting text shows text', () => {
    // Act.
    moduleHeader.chipText = 'foo';
    moduleHeader.descriptionText = 'bar';
    moduleHeader.showDismissButton = true;
    moduleHeader.dismissText = 'baz';
    moduleHeader.disableText = 'abc';
    render(moduleHeader);

    // Assert.
    assertEquals('foo', $$(moduleHeader, '#chip').textContent.trim());
    assertEquals('bar', $$(moduleHeader, '#description').textContent.trim());
    assertEquals('baz', $$(moduleHeader, '#dismissButton').textContent.trim());
    assertEquals('abc', $$(moduleHeader, '#disableButton').textContent.trim());
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
    $$(moduleHeader, '#infoButton').click();
    $$(moduleHeader, '#dismissButton').click();
    $$(moduleHeader, '#disableButton').click();
    $$(moduleHeader, '#customizeButton').click();

    // Assert.
    assertTrue(infoButtonClick.received);
    assertTrue(dismissButtonClick.received);
    assertTrue(disableButtonClick.received);
    assertTrue(customizeModule.received);
  });

  test('action menu opens and closes', () => {
    // Act & Assert.
    assertFalse($$(moduleHeader, '#actionMenu').open);
    $$(moduleHeader, '#menuButton').click();
    assertTrue($$(moduleHeader, '#actionMenu').open);
    $$(moduleHeader, '#disableButton').click();
    assertFalse($$(moduleHeader, '#actionMenu').open);
  });
});
