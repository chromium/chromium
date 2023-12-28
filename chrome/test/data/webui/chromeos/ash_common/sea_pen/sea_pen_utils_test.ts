// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isImageDataUrl, isNonEmptyFilePath} from 'chrome://resources/ash/common/sea_pen/sea_pen_utils.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('isImageDataUrlTest', function() {
  test('invalid types', function() {
    for (const value of [null, undefined, false, 'not_url', 0, 1, {}]) {
      assertFalse(isImageDataUrl(value), `${value} is not Url`);
    }
  });

  test('Url but not image data Url', function() {
    assertFalse(isImageDataUrl(
        {url: 'data:text/html,%3Ch1%3EHello%2C%20World%21%3C%2Fh1%3E'}));
  });

  test('image data url with invalid jpg type', function() {
    assertFalse(
        isImageDataUrl({url: 'data:image/jpg;base64,a'}),
        'image/jpg should be image/jpeg');
  });

  test('valid image data url', function() {
    assertTrue(isImageDataUrl({url: 'data:image/jpeg;base64,a'}));
    assertTrue(isImageDataUrl({url: 'data:image/png;base64,a'}));
  });
});

suite('isNonEmptyFilePathTest', function() {
  test('invalid types', function() {
    for (const value of [null, undefined, false, 'not_file_path', 0, 1, {}]) {
      assertFalse(isNonEmptyFilePath(value), `${value} is not FilePath`);
    }
  });

  test('empty path', function() {
    assertFalse(isNonEmptyFilePath({path: null}), 'null path invalid');
    assertFalse(
        isNonEmptyFilePath({path: undefined}), 'undefined path invalid');
    assertFalse(isNonEmptyFilePath({path: ''}), 'empty path invalid');
  });

  test('Non-empty path', function() {
    assertTrue(isNonEmptyFilePath({path: 'a'}));
  });
});
