// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createScrollBorders, decodeString16, getTrustedHTML, mojoString16} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('scroll borders', () => {
  let container: HTMLElement;
  let content: HTMLElement;
  let top: HTMLElement;
  let bottom: HTMLElement;
  let observer: IntersectionObserver;

  function assertHidden(el: HTMLElement) {
    assertTrue(el.matches('[scroll-border]:not([show])'));
  }

  function assertShown(el: HTMLElement) {
    assertTrue(el.matches('[scroll-border][show]'));
  }

  setup(async () => {
    document.body.innerHTML = getTrustedHTML`
        <div scroll-border></div>
        <div id="container"><div id="content"></div></div>
        <div scroll-border></div>`;
    container = document.body.querySelector<HTMLElement>('#container')!;
    container.style.height = '100px';
    container.style.overflow = 'auto';
    content = document.body.querySelector<HTMLElement>('#content')!;
    content.style.height = '200px';
    top = document.body.firstElementChild as HTMLElement;
    bottom = document.body.lastElementChild as HTMLElement;
    observer = createScrollBorders(container, top, bottom, 'show');
    await waitAfterNextRender(document.body);
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
    await waitAfterNextRender(document.body);
    await flushTasks();
    assertShown(top);
    assertShown(bottom);
  });

  test('bottom border hidden when no content available below', async () => {
    container.scrollTop = 200;
    await waitAfterNextRender(document.body);
    await flushTasks();
    assertShown(top);
    assertHidden(bottom);
  });

  test('borders hidden when all content is shown', async () => {
    content.style.height = '100px';
    await waitAfterNextRender(document.body);
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
