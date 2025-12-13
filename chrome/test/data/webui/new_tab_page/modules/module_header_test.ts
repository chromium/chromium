// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleHeaderElementV2} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ModuleHeaderV2', () => {
  let moduleHeaderElementV2: ModuleHeaderElementV2;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    moduleHeaderElementV2 = new ModuleHeaderElementV2();
    document.body.appendChild(moduleHeaderElementV2);
  });

  test('title only shows if headerText is not null', async () => {
    // Assert.
    assertFalse(isVisible(moduleHeaderElementV2.$.title));

    // Act.
    moduleHeaderElementV2.headerText = 'foo';
    await microtasksFinished();

    // Assert
    assertTrue(isVisible(moduleHeaderElementV2.$.title));
  });

  test('clicking the menu button shows the action menu', async () => {
    // Act.
    moduleHeaderElementV2.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    assertTrue(moduleHeaderElementV2.$.actionMenu.open);
  });

  test('menu items are displayed', async () => {
    // Act.
    moduleHeaderElementV2.menuItems = [
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
    moduleHeaderElementV2.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    const dropDownItems =
        moduleHeaderElementV2.shadowRoot.querySelectorAll('button');
    assertEquals(3, dropDownItems.length);
    assertEquals('foo', dropDownItems[0]!.id);
    assertEquals('bar', dropDownItems[1]!.id);
    assertEquals('customize-module', dropDownItems[2]!.id);
  });

  test('customize modules hides if `hideCustomize` is set', async () => {
    // Arrange.
    moduleHeaderElementV2.menuItems = [
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
    moduleHeaderElementV2.$.menuButton.click();
    await microtasksFinished();
    let dropDownItems =
        moduleHeaderElementV2.shadowRoot.querySelectorAll('button');
    assertEquals(3, dropDownItems.length);
    assertEquals('customize-module', dropDownItems[2]!.id);
    const horizontalRule = $$(moduleHeaderElementV2, 'hr');
    assertTrue(isVisible(horizontalRule));

    // Act.
    moduleHeaderElementV2.hideCustomize = true;
    await microtasksFinished();

    // Assert.
    dropDownItems = moduleHeaderElementV2.shadowRoot.querySelectorAll('button');
    assertEquals(2, dropDownItems.length);
    assertFalse(isVisible(horizontalRule));
  });


  test('horizontal rule shows if `menuItems` is not empty', async () => {
    // Act.
    moduleHeaderElementV2.menuItems = [
      {
        action: 'baz',
        icon: 'modules:baz',
        text: 'Baz',
      },
    ];
    moduleHeaderElementV2.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    const horizontalRule = $$(moduleHeaderElementV2, 'hr');
    assertTrue(isVisible(horizontalRule));
  });

  test('horizontal rule hides if `menuItems` is empty', async () => {
    // Act.
    moduleHeaderElementV2.$.menuButton.click();
    await microtasksFinished();

    // Assert.
    const dropDownItems =
        moduleHeaderElementV2.shadowRoot.querySelectorAll('button');
    assertEquals(1, dropDownItems.length);
    const horizontalRule = $$(moduleHeaderElementV2, 'hr');
    assertFalse(isVisible(horizontalRule));
  });
});
