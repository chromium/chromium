// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('CrAutoImgElementTest', () => {
  let img: CrAutoImgElement;

  setup(() => {
    document.body.innerHTML = '';
    img = new CrAutoImgElement();
    document.body.appendChild(img);
  });

  ([
    ['https://foo.com/img.png', 'chrome://image/?https://foo.com/img.png'],
    ['chrome://foo/img.png', 'chrome://foo/img.png'],
    ['data:imge/png;base64,abc', 'data:imge/png;base64,abc'],
    ['', ''],
    ['chrome-untrusted://foo/img.png', ''],
  ] as [string, string][])
      .forEach(([autoSrc, src]) => {
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
