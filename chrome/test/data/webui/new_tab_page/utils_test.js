// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// So that mojo is defined.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {createScrollBorders, decodeString16, mojoString16} from 'chrome://new-tab-page/new_tab_page.js';
import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';

suite('scroll borders', () => {
  /** @type {!HTMLElement} */
  let container;
  /** @type {!HTMLElement} */
  let content;
  /** @type {!HTMLElement} */
  let top;
  /** @type {!HTMLElement} */
  let bottom;
  /** @type {!IntersectionObserver} */
  let observer;

  /** @param {!HTMLElement} el */
  function assertHidden(el) {
    assertTrue(el.matches('[scroll-border]:not([show])'));
  }

  /** @param {!HTMLElement} el */
  function assertShown(el) {
    assertTrue(el.matches('[scroll-border][show]'));
  }

  setup(async () => {
    document.body.innerHTML = `
        <div scroll-border></div>
        <div id="container"><div id="content"></div></div>
        <div scroll-border></div>`;
    container = document.querySelector('#container');
    container.style.height = '100px';
    container.style.overflow = 'auto';
    content = document.querySelector('#content');
    content.style.height = '200px';
    top = document.body.firstElementChild;
    bottom = document.body.lastElementChild;
    observer = createScrollBorders(container, top, bottom, 'show');
    await waitAfterNextRender();
    await flushTasks();
  });

  teardown(() => {
    observer.disconnect();
  });

  test('bottom border show when more content available below', () => {
    assertHidden(top);
    assertShown(bottom);
  });

  test('borders shown when content available above and below', async () => {
    container.scrollTop = 10;
    await waitAfterNextRender();
    await flushTasks();
    assertShown(top);
    assertShown(bottom);
  });

  test('bottom border hidden when no content available below', async () => {
    container.scrollTop = 200;
    await waitAfterNextRender();
    await flushTasks();
    assertShown(top);
    assertHidden(bottom);
  });

  test('borders hidden when all content is shown', async () => {
    content.style.height = '100px';
    await waitAfterNextRender();
    await flushTasks();
    assertHidden(top);
    assertHidden(bottom);
  });
});

suite('Mojo type conversions', () => {
  test('Can convert JavaScript string to Mojo String16 and back', () => {
    assertEquals('hello world', decodeString16(mojoString16('hello world')));
  });
});
