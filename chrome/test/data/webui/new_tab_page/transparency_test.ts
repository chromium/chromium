// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {checkTransparency, isBMP, isPNG, isWebP} from 'chrome://new-tab-page/new_tab_page.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {OPAQUE_BMP_FILE, OPAQUE_PNG_FILE, OPAQUE_WEBP_FILE, TRANSPARENT_BMP_FILE, TRANSPARENT_PNG_FILE, TRANSPARENT_WEBP_FILE} from './transparency_test_support.js';

suite('NewTabPageTransparency', () => {
  test('isBMP returns true when fed bmp', () => {
    [TRANSPARENT_BMP_FILE, OPAQUE_BMP_FILE].forEach(async (file: File) => {
      const buffer = await file.arrayBuffer();
      assertTrue(isBMP(new DataView(buffer)));
    });
  });

  test('isBMP returns false when fed non-bmp', () => {
    [TRANSPARENT_PNG_FILE, TRANSPARENT_WEBP_FILE, OPAQUE_PNG_FILE,
     OPAQUE_WEBP_FILE]
        .forEach(async (file: File) => {
          const buffer = await file.arrayBuffer();
          assertFalse(isBMP(new DataView(buffer)));
        });
  });

  test('isWebP returns true when fed webp', () => {
    [TRANSPARENT_WEBP_FILE, OPAQUE_WEBP_FILE].forEach(async (file: File) => {
      const buffer = await file.arrayBuffer();
      assertTrue(isWebP(new DataView(buffer)));
    });
  });

  test('isWebP returns false when fed non-webp', () => {
    [TRANSPARENT_PNG_FILE, TRANSPARENT_BMP_FILE, OPAQUE_PNG_FILE,
     OPAQUE_BMP_FILE]
        .forEach(async (file: File) => {
          const buffer = await file.arrayBuffer();
          assertFalse(isWebP(new DataView(buffer)));
        });
  });

  test('isPNG returns true when fed png', () => {
    [TRANSPARENT_PNG_FILE, OPAQUE_PNG_FILE].forEach(async (file: File) => {
      const buffer = await file.arrayBuffer();
      assertTrue(isPNG(new DataView(buffer)));
    });
  });

  test('isPNG returns false when fed non-png', () => {
    [TRANSPARENT_BMP_FILE, TRANSPARENT_WEBP_FILE, OPAQUE_BMP_FILE,
     OPAQUE_WEBP_FILE]
        .forEach(async (file: File) => {
          const buffer = await file.arrayBuffer();
          assertFalse(isPNG(new DataView(buffer)));
        });
  });

  test('checkTransparency returns false when fed opaque images', () => {
    [OPAQUE_PNG_FILE, OPAQUE_BMP_FILE, OPAQUE_WEBP_FILE].forEach(
        async (file: File) => {
          const buffer = await file.arrayBuffer();
          assertFalse(checkTransparency(buffer));
        });
  });

  test(
      'checkTransparency returns true when fed transparent images', () => {
        [TRANSPARENT_BMP_FILE, TRANSPARENT_PNG_FILE, TRANSPARENT_WEBP_FILE]
            .forEach(async (file: File) => {
              const buffer = await file.arrayBuffer();
              assertTrue(checkTransparency(buffer));
            });
      });

  test('checkTransparency returns false when image is empty', () => {
    const buffer = new ArrayBuffer(0);
    assertFalse(checkTransparency(buffer));
  });

  test('checkTransparency returns false when fed empty png', () => {
    // Magic number for PNG header.
    const {buffer} = new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]);

    // Consistency check.
    assertTrue(isPNG(new DataView(buffer)));
    assertFalse(checkTransparency(buffer));
  });

  test('checkTransparency returns false when fed empty webp', () => {
    // Magic number for WebP header.
    const {buffer} =
        new Uint8Array([82, 73, 70, 70, 0, 0, 0, 0, 87, 69, 66, 80]);

    // Consistency check.
    assertTrue(isWebP(new DataView(buffer)));
    assertFalse(checkTransparency(buffer));
  });

  test('checkTransparency returns false wehn fed empty bmp', () => {
    // Magic number for BMP header.
    const {buffer} = new Uint8Array([66, 77]);

    // Consistency check.
    assertTrue(isBMP(new DataView(buffer)));
    assertFalse(checkTransparency(buffer));
  });
});
