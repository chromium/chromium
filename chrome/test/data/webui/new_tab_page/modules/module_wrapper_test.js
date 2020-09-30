// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';

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
      heightPx: 100,
      title: 'Foo Title',
      element: moduleElement,
    };

    // Assert.
    assertEquals('Foo Title', moduleWrapper.$.title.textContent);
    assertEquals(100, $$(moduleWrapper, '#moduleElement').offsetHeight);
    assertDeepEquals(
        moduleElement, $$(moduleWrapper, '#moduleElement').children[0]);
  });

  test('descriptor can only be set once', () => {
    const moduleElement = document.createElement('div');
    moduleWrapper.descriptor = {
      id: 'foo',
      heightPx: 100,
      title: 'Foo Title',
      element: moduleElement,
    };
    assertThrows(() => {
      moduleWrapper.descriptor = {
        id: 'foo',
        heightPx: 100,
        title: 'Foo Title',
        element: moduleElement,
      };
    });
  });
});
