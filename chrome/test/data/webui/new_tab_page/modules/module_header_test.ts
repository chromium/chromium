// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleHeaderElementV2} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ModuleHeaderV2', () => {
  let moduleHeader: ModuleHeaderElementV2;

  setup(() => {
    loadTimeData.overrideValues({hideDismissModules: true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    moduleHeader = new ModuleHeaderElementV2();
    document.body.appendChild(moduleHeader);
  });

  test('title only shows if headerText is not null', async () => {
    // Assert.
    assertFalse(isVisible(moduleHeader.$.title));

    // Act.
    moduleHeader.headerText = 'foo';
    await microtasksFinished();

    // Assert
    assertTrue(isVisible(moduleHeader.$.title));
  });

  test('clicking the menu button shows the action menu', async () => {
    // Act.
    moduleHeader.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    assertTrue(moduleHeader.$.actionMenu.open);
  });

  test('menu items are displayed', async () => {
    // Act.
    moduleHeader.menuItems = [
      {
        action: 'foo',
        icon: 'modules:foo',
        text: 'Foo',
      },
      {
        action: 'bar',
        icon: 'modules:bar',
        text: 'Bar',
      },
    ];
    moduleHeader.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    const dropDownItems = moduleHeader.shadowRoot.querySelectorAll('button');
    assertEquals(3, dropDownItems.length);
    assertEquals('foo', dropDownItems[0]!.id);
    assertEquals('bar', dropDownItems[1]!.id);
    assertEquals('customize-module', dropDownItems[2]!.id);
  });

  test('customize modules hides if `hideCustomize` is set', async () => {
    // Arrange.
    moduleHeader.menuItems = [
      {
        action: 'foo',
        icon: 'modules:foo',
        text: 'Foo',
      },
      {
        action: 'bar',
        icon: 'modules:bar',
        text: 'Bar',
      },
    ];
    moduleHeader.$.menuButton.click();
    await microtasksFinished();
    let dropDownItems = moduleHeader.shadowRoot.querySelectorAll('button');
    assertEquals(3, dropDownItems.length);
    assertEquals('customize-module', dropDownItems[2]!.id);
    const horizontalRule = $$(moduleHeader, 'hr');
    assertTrue(isVisible(horizontalRule));

    // Act.
    moduleHeader.hideCustomize = true;
    await microtasksFinished();

    // Assert.
    dropDownItems = moduleHeader.shadowRoot.querySelectorAll('button');
    assertEquals(2, dropDownItems.length);
    assertFalse(isVisible(horizontalRule));
  });


  test('horizontal rule shows if `menuItems` is not empty', async () => {
    // Act.
    moduleHeader.menuItems = [
      {
        action: 'baz',
        icon: 'modules:baz',
        text: 'Baz',
      },
    ];
    moduleHeader.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    const horizontalRule = $$(moduleHeader, 'hr');
    assertTrue(isVisible(horizontalRule));
  });

  test('horizontal rule hides if `menuItems` is empty', async () => {
    // Act.
    moduleHeader.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    const dropDownItems = moduleHeader.shadowRoot.querySelectorAll('button');
    assertEquals(1, dropDownItems.length);
    const horizontalRule = $$(moduleHeader, 'hr');
    assertFalse(isVisible(horizontalRule));
  });

  test('dismiss action is hidden if `hideDismissModules` is true', async () => {
    moduleHeader.menuItems = [
      {
        action: 'dismiss',
        icon: 'modules:dismiss',
        text: 'Dismiss',
      },
      {
        action: 'disable',
        icon: 'modules:disable',
        text: 'Disable',
      },
    ];
    moduleHeader.$.menuButton.click();
    await microtasksFinished();
    const dropDownItems = moduleHeader.shadowRoot.querySelectorAll('button');
    assertEquals(2, dropDownItems.length);
    assertEquals('disable', dropDownItems[0]!.id);
    assertEquals('customize-module', dropDownItems[1]!.id);
  });
});
