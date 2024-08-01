// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';

import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';
import {assertEquals, assertFalse, assertNotEquals, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-drawer', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createDrawer(align: string): CrDrawerElement {
    document.body.innerHTML = getTrustedHtml(`
      <cr-drawer id="drawer" align="${align}">
        <div slot="body">Test content</div>
      </cr-drawer>
    `);
    return document.body.querySelector('cr-drawer')!;
  }

  test('open and close', async function() {
    const drawer = createDrawer('ltr');
    const waits = Promise.all(['cr-drawer-opening', 'cr-drawer-opened'].map(
        eventName => eventToPromise(eventName, drawer)));
    drawer.openDrawer();

    await waits;
    assertTrue(drawer.open);

    // Clicking the content does not close the drawer.
    document.body.querySelector<HTMLElement>('div[slot="body"]')!.click();

    const whenClosed = eventToPromise('close', drawer);
    drawer.$.dialog.dispatchEvent(new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      clientX: 300,  // Must be larger than the drawer width (256px).
      clientY: 300,
    }));

    await whenClosed;
    assertFalse(drawer.open);
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);
    drawer.close();
    return eventToPromise('close', drawer);
  });

  test('align=ltr', async function() {
    const drawer = createDrawer('ltr');
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);
    const rect = drawer.$.dialog.getBoundingClientRect();
    assertEquals(0, rect.left);
    assertNotEquals(0, rect.right);
  });

  test('align=rtl', async function() {
    const drawer = createDrawer('rtl');
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);
    const rect = drawer.$.dialog.getBoundingClientRect();
    assertNotEquals(0, rect.left);
    assertEquals(window.innerWidth, rect.right);
  });

  test('close and cancel', async () => {
    const drawer = createDrawer('ltr');
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);
    assertFalse(drawer.wasCanceled());
    drawer.cancel();
    await eventToPromise('close', drawer);
    assertTrue(drawer.wasCanceled());
    drawer.openDrawer();
    assertFalse(drawer.wasCanceled());
    await eventToPromise('cr-drawer-opened', drawer);
    drawer.close();
    await eventToPromise('close', drawer);
    assertFalse(drawer.wasCanceled());
    drawer.toggle();
    assertFalse(drawer.wasCanceled());
    return eventToPromise('cr-drawer-opened', drawer);
  });

  test('openDrawer/close/toggle can be called multiple times in a row', () => {
    const drawer = createDrawer('ltr');
    drawer.openDrawer();
    drawer.close();
    drawer.close();
    drawer.openDrawer();
    drawer.openDrawer();
    drawer.toggle();
    drawer.toggle();
    drawer.toggle();
    drawer.toggle();
  });

  test('cannot set open', () => {
    const drawer = createDrawer('ltr');
    assertThrows(() => {
      drawer.open = true;
    });
  });
});
