// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {IconTable, IconType} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {IconFromTableElement} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('IconFromTableTest', function() {
  let iconFromTable: IconFromTableElement;
  let iconTable: IconTable;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    iconTable = IconTable.getInstance();
    iconFromTable = document.createElement('icon-from-table');
    document.body.appendChild(iconFromTable);
  });

  teardown(() => {
    iconTable.reset();
  });

  test('Rendering icons', async function() {
    iconTable.applyUpdates([
      {
        handleId: 1n,
        iconUrlOrName: 'cr:schedule',
        iconType: IconType.kIconSet,
      },
      {
        handleId: 2n,
        iconUrlOrName: 'puppy.svg',
        iconType: IconType.kMaskUrl,
      },
      {
        handleId: 3n,
        iconUrlOrName: 'parrot.png',
        iconType: IconType.kFullColorUrl,
      },
    ]);

    iconFromTable.style.setProperty('--icon-size', '45px');

    {
      iconFromTable.iconHandle = {
        handleId: 1n,
      };
      await microtasksFinished();

      const renderer =
          iconFromTable.shadowRoot.querySelector<CrIconElement>('cr-icon');
      assertTrue(!!renderer);
      const dims = renderer.getBoundingClientRect();
      assertEquals(45, dims.width);
      assertEquals(45, dims.height);
      assertEquals('cr:schedule', renderer.icon);
    }

    {
      iconFromTable.iconHandle = {
        handleId: 2n,
      };
      await microtasksFinished();

      const renderer =
          iconFromTable.shadowRoot.querySelector('#maskIconContainer');
      assertTrue(!!renderer);
      const dims = renderer.getBoundingClientRect();
      assertEquals(45, dims.width);
      assertEquals(45, dims.height);
      const style = renderer.computedStyleMap();
      assertEquals(
          'url("chrome://webui-toolbar.top-chrome/puppy.svg")',
          style.get('mask-image')?.toString());
    }

    {
      iconFromTable.iconHandle = {
        handleId: 3n,
      };
      await microtasksFinished();

      const renderer =
          iconFromTable.shadowRoot.querySelector('#colorfulIconContainer');
      assertTrue(!!renderer);
      const dims = renderer.getBoundingClientRect();
      assertEquals(45, dims.width);
      assertEquals(45, dims.height);
      const style = renderer.computedStyleMap();
      assertEquals(
          'url("chrome://webui-toolbar.top-chrome/parrot.png")',
          style.get('background-image')?.toString());
    }
  });
});
