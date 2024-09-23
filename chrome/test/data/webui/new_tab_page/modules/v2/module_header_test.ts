// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleHeaderElementV2} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ModuleHeaderV2', () => {
  let moduleHeaderElementV2: ModuleHeaderElementV2;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    moduleHeaderElementV2 = new ModuleHeaderElementV2();
    document.body.appendChild(moduleHeaderElementV2);
  });

  test('menu items are displayed', async () => {
    // Act
    moduleHeaderElementV2.menuItemGroups = [
      [
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
      ],
      [
        {
          action: 'baz',
          icon: 'modules:baz',
          text: 'Baz',
        },
      ],
    ];
    await microtasksFinished();
    // Assert.
    const dropDownItems =
        moduleHeaderElementV2.shadowRoot!.querySelectorAll('button');
    assertEquals(3, dropDownItems.length);
    assertEquals('foo', dropDownItems[0]!.id);
    assertEquals('bar', dropDownItems[1]!.id);
    assertEquals('baz', dropDownItems[2]!.id);
  });

  test(
      'horizontal rule shows if the first list of menu items is not empty',
      async () => {
        // Act
        moduleHeaderElementV2.menuItemGroups = [
          [
            {
              action: 'baz',
              icon: 'modules:baz',
              text: 'Baz',
            },
          ],
          [],
        ];
        await microtasksFinished();
        // Assert.
        const horizontalRule =
            moduleHeaderElementV2.shadowRoot!.querySelector('hr');
        assertTrue(!!horizontalRule);
        assertFalse(horizontalRule.hidden);
      });

  test(
      'horizontal rule is hidden if the first list of menu items is empty',
      async () => {
        // Act
        moduleHeaderElementV2.menuItemGroups = [
          [],
          [
            {
              action: 'foo',
              icon: 'modules:foo',
              text: 'Foo',
            },
          ],
        ];
        await microtasksFinished();
        // Assert.
        const horizontalRule =
            moduleHeaderElementV2.shadowRoot!.querySelector('hr');
        assertTrue(!!horizontalRule);
        assertFalse(horizontalRule.hidden);
      });
});
