// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ImgElement} from 'chrome://new-tab-page/new_tab_page.js';

import {assertEquals} from '../chai_assert.js';

suite('NewTabPageImgTest', () => {
  /** @type {!ImgElement} */
  let img;

  setup(() => {
    document.body.innerHTML = '';
    img = new ImgElement();
    document.body.appendChild(img);
  });

  test('setting externalSrc sets src', () => {
    // Act.
    img.externalSrc = 'foo.com/img.png';

    // Assert.
    assertEquals('foo.com/img.png', img.externalSrc);
    assertEquals('foo.com/img.png', img.getAttribute('external-src'));
    assertEquals('chrome://image/?foo.com/img.png', img.src);
  });

  test('setting external-src sets src', () => {
    // Act.
    img.setAttribute('external-src', 'foo.com/img.png');

    // Assert.
    assertEquals('foo.com/img.png', img.externalSrc);
    assertEquals('foo.com/img.png', img.getAttribute('external-src'));
    assertEquals('chrome://image/?foo.com/img.png', img.src);
  });
});
