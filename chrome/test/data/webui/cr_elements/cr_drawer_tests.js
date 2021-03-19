// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertNotEquals, assertThrows, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-drawer', function() {
  setup(function() {
    document.body.innerHTML = '';
  });

  /**
   * @param {string} align
   * @return {!CrDrawerElement}
   */
  function createDrawer(align) {
    document.body.innerHTML = `
      <cr-drawer id="drawer" align="${align}">
        <div class="drawer-header">Test</div>
        <div class="drawer-content">Test content</div>
      </cr-drawer>
    `;
    flush();
    return /** @type {!CrDrawerElement} */ (document.getElementById('drawer'));
  }

  test('open and close', function() {
    const drawer = createDrawer('ltr');
    const waits = Promise.all(['cr-drawer-opening', 'cr-drawer-opened'].map(
        eventName => eventToPromise(eventName, drawer)));
    drawer.openDrawer();

    return waits
        .then(() => {
          assertTrue(drawer.open);

          // Clicking the content does not close the drawer.
          document.querySelector('.drawer-content').click();

          const whenClosed = eventToPromise('close', drawer);
          drawer.$$('#dialog').dispatchEvent(new MouseEvent('click', {
            bubbles: true,
            cancelable: true,
            clientX: 300,  // Must be larger than the drawer width (256px).
            clientY: 300,
          }));

          return whenClosed;
        })
        .then(() => {
          assertFalse(drawer.open);
          drawer.openDrawer();
          return eventToPromise('cr-drawer-opened', drawer);
        })
        .then(() => {
          drawer.close();
          return eventToPromise('close', drawer);
        });
  });

  test('clicking icon closes drawer', async () => {
    // Create a drawer with an icon and open it.
    document.body.innerHTML = `
      <cr-drawer id="drawer" align="ltr" icon-name="menu" icon-title="close">
      </cr-drawer>
    `;
    flush();
    const drawer = document.getElementById('drawer');
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);

    // Clicking the icon closes the drawer.
    drawer.$$('#iconButton').click();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
    assertTrue(drawer.wasCanceled());
  });

  test('align=ltr', function() {
    const drawer = createDrawer('ltr');
    drawer.openDrawer();
    return eventToPromise('cr-drawer-opened', drawer).then(() => {
      const rect = drawer.$$('#dialog').getBoundingClientRect();
      assertEquals(0, rect.left);
      assertNotEquals(0, rect.right);
    });
  });

  test('align=rtl', function() {
    const drawer = createDrawer('rtl');
    drawer.openDrawer();
    return eventToPromise('cr-drawer-opened', drawer).then(() => {
      const rect = drawer.$$('#dialog').getBoundingClientRect();
      assertNotEquals(0, rect.left);
      assertEquals(window.innerWidth, rect.right);
    });
  });

  test('close and cancel', () => {
    const drawer = createDrawer('ltr');
    drawer.openDrawer();
    return eventToPromise('cr-drawer-opened', drawer)
        .then(() => {
          assertFalse(drawer.wasCanceled());
          drawer.cancel();
          return eventToPromise('close', drawer);
        })
        .then(() => {
          assertTrue(drawer.wasCanceled());
          drawer.openDrawer();
          assertFalse(drawer.wasCanceled());
          return eventToPromise('cr-drawer-opened', drawer);
        })
        .then(() => {
          drawer.close();
          return eventToPromise('close', drawer);
        })
        .then(() => {
          assertFalse(drawer.wasCanceled());
          drawer.toggle();
          assertFalse(drawer.wasCanceled());
          return eventToPromise('cr-drawer-opened', drawer);
        });
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
