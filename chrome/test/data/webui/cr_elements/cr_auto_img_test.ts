// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

async function waitForAttributeChange(
    element: HTMLElement, attribute: string): Promise<Array<MutationRecord>> {
  return new Promise(resolve => {
    const observer = new MutationObserver((mutations, obs) => {
      obs.disconnect();
      resolve(mutations);
    });
    observer.observe(
        element,
        {
          attributes: true,
          attributeFilter: [attribute],
          attributeOldValue: true,
          childList: false,
          subtree: false,
        },
    );
  });
}

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

  test(
      'setting withCookies creates a URL with autoSrc and withCookies as params',
      () => {
        const autoSrc = 'https://foo.com/img.png';

        // Act.
        img.withCookies = true;
        img.autoSrc = autoSrc;

        // Assert.
        assertEquals(
            `chrome://image/?url=${
                encodeURIComponent(autoSrc)}&withCookies=true`,
            img.src);

        // Act.
        img.withCookies = false;

        // Assert.
        assertEquals(`chrome://image/?${autoSrc}`, img.src);
      });

  test(
      'setting with-cookies creates a URL with autoSrc and withCookies as params',
      () => {
        const autoSrc = 'https://foo.com/img.png';

        // Act.
        img.setAttribute('with-cookies', '');
        img.autoSrc = autoSrc;

        // Assert.
        assertEquals(
            `chrome://image/?url=${
                encodeURIComponent(autoSrc)}&withCookies=true`,
            img.src);

        // Act.
        img.removeAttribute('with-cookies');

        // Assert.
        assertEquals(`chrome://image/?${autoSrc}`, img.src);
      });

  test(
      'setting clear-src removes the src attribute first when auto-src changes',
      async () => {
        const originalSrc = 'chrome://foo/foo.png';
        img.clearSrc = '';
        img.autoSrc = originalSrc;
        assertEquals(
            originalSrc, img.src, 'src attribute is set to initial value');

        const newSrc = 'chrome://bar/bar.png';

        const attrChangedPromise = waitForAttributeChange(img, 'src');
        img.autoSrc = newSrc;

        const mutations = await attrChangedPromise;
        assertEquals(2, mutations.length, 'src is changed twice');
        assertEquals(
            originalSrc, mutations[0]?.oldValue,
            'src starts as original value');

        assertEquals(
            null, mutations[1]?.oldValue, 'src is set to null in between');

        assertEquals(newSrc, img.src, 'src attribute is set to new value');
      });
});
