// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {processFile} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertLE, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

import {generateTestImageFile, IMAGE_FILE} from './image_processor_test_support.js';

suite('NewTabPageImageProcessorTest', () => {
  test('processFile will not downscale small images', async () => {
    const originalFile = await generateTestImageFile(100, 100, 'image/jpeg');
    const processFileResult = await processFile(originalFile);

    assertNotEquals(processFileResult.imageWidth, undefined);
    assertNotEquals(processFileResult.imageHeight, undefined);
    const imageWidth = processFileResult.imageWidth!;
    const imageHeight = processFileResult.imageHeight!;

    assertEquals(imageWidth, 100);
    assertEquals(imageHeight, 100);
  });

  test('processFile will downscale large images', async () => {
    const originalFile = await generateTestImageFile(250, 250, 'image/png');
    const processFileResult = await processFile(originalFile, 100);
    assertNotEquals(processFileResult.imageWidth, undefined);
    assertNotEquals(processFileResult.imageHeight, undefined);
    const imageWidth = processFileResult.imageWidth!;
    const imageHeight = processFileResult.imageHeight!;

    assertLE(imageWidth, 100);
    assertLE(imageHeight, 100);
  });

  test('returns original file when processed file is larger', async () => {
    // IMAGE_FILE is more efficient as a PNG than JPEG.
    const originalFile = IMAGE_FILE;
    const {processedFile, imageWidth, imageHeight} =
        await processFile(originalFile, 100);

    assertEquals(processedFile, originalFile);
    assertEquals(imageWidth!, 225);
    assertEquals(imageHeight!, 225);
  });
});
