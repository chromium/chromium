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

  [['https://foo.com/img.png', 'chrome://image/?https://foo.com/img.png'],
   ['chrome://foo/img.png', 'chrome://foo/img.png'],
   ['data:imge/png;base64,abc', 'data:imge/png;base64,abc'],
   ['', ''],
   ['chrome-untrusted://foo/img.png', ''],
  ].forEach(([autoSrc, src]) => {
    test(`setting autoSrc to '${autoSrc}' sets src to '${src}'`, () => {
      // Act.
      img.autoSrc = autoSrc;

      // Assert.
      assertEquals(autoSrc, img.autoSrc);
      assertEquals(autoSrc, img.getAttribute('auto-src'));
      assertEquals(src, img.src);
    });

    test(`setting auto-src to '${autoSrc}' sets src to '${src}'`, () => {
      // Act.
      img.setAttribute('auto-src', autoSrc);

      // Assert.
      assertEquals(autoSrc, img.autoSrc);
      assertEquals(autoSrc, img.getAttribute('auto-src'));
      assertEquals(src, img.src);
    });
  });
});
