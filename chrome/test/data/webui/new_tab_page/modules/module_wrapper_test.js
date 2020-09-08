// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertNotStyle, assertStyle} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageModulesModuleWrapperTest', () => {
  /** @type {!ModuleWrapperElement} */
  let moduleWrapper;

  setup(() => {
    PolymerTest.clearBody();
    moduleWrapper = document.createElement('ntp-module-wrapper');
    document.body.appendChild(moduleWrapper);
  });

  test('renders module descriptor', async () => {
    // Arrange.
    const moduleElement = document.createElement('div');

    // Act.
    moduleWrapper.descriptor = {
      id: 'foo',
      name: 'Foo',
      heightPx: 100,
      title: 'Foo Title',
      element: moduleElement,
    };

    // Assert.
    assertEquals('Foo Title', moduleWrapper.$.title.textContent);
    assertEquals('Foo', moduleWrapper.$.name.textContent);
    assertEquals(100, $$(moduleWrapper, '#moduleElement').offsetHeight);
    assertDeepEquals(
        moduleElement, $$(moduleWrapper, '#moduleElement').children[0]);
  });

  test('collapses and expands', () => {
    // Arrange.
    const moduleElement = document.createElement('div');
    moduleWrapper.descriptor = {
      id: 'foo',
      name: 'Foo',
      heightPx: 100,
      title: 'Foo Title',
      element: moduleElement,
    };

    // Act (collapse).
    moduleWrapper.$.toggleButton.click();

    // Assert (collapsed).
    assertStyle(moduleWrapper.$.title, 'display', 'none');
    assertStyle(moduleWrapper.$.dot, 'display', 'none');
    assertStyle(moduleWrapper.$.moduleElement, 'display', 'none');
    assertStyle(
        moduleWrapper.$.toggleButton, '--cr-icon-image',
        'url("chrome://resources/images/icon_expand_more.svg")');
    assertEquals(
        moduleWrapper.$.header.offsetHeight, moduleWrapper.offsetHeight);

    // Act (expand).
    moduleWrapper.$.toggleButton.click();

    // Assert (expanded).
    assertNotStyle(moduleWrapper.$.title, 'display', 'none');
    assertNotStyle(moduleWrapper.$.dot, 'display', 'none');
    assertNotStyle(moduleWrapper.$.moduleElement, 'display', 'none');
    assertStyle(
        moduleWrapper.$.toggleButton, '--cr-icon-image',
        'url("chrome://resources/images/icon_expand_less.svg")');
    assertEquals(
        moduleWrapper.$.header.offsetHeight + 100, moduleWrapper.offsetHeight);
  });
});
