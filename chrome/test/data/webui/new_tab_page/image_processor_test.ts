// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hasC2paMetadata, processFile} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertLE, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {generateTestC2paImageFile, generateTestImageFile, IMAGE_FILE} from './image_processor_test_support.js';

suite('NewTabPageImageProcessorTest', () => {
  setup(() => {
    loadTimeData.overrideValues({
      lensBypassCompressionForC2pa: true,
    });
  });

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

  test('hasC2paMetadata detects C2PA marker in file', async () => {
    const c2paFile = await generateTestC2paImageFile(100, 100, 'image/jpeg');
    const hasC2pa = await hasC2paMetadata(c2paFile);
    assertTrue(hasC2pa);

    const normalFile = await generateTestImageFile(100, 100, 'image/jpeg');
    const hasNoC2pa = await hasC2paMetadata(normalFile);
    assertFalse(hasNoC2pa);
  });

  test(
      'bypasses compression when C2PA metadata is present and <= 3MP',
      async () => {
        // 250x250 image would normally be downscaled if maxLongestEdgePixels is
        // 100.
        const originalFile =
            await generateTestC2paImageFile(250, 250, 'image/jpeg');
        const {processedFile, imageWidth, imageHeight} =
            await processFile(originalFile, 100);

        assertEquals(processedFile, originalFile);
        assertEquals(imageWidth, 250);
        assertEquals(imageHeight, 250);
      });

  test('does not bypass compression when C2PA image exceeds 3MP', async () => {
    // 2000x1600 = 3,200,000 pixels (> 3,000,000 max C2PA pixels).
    const originalFile =
        await generateTestC2paImageFile(2000, 1600, 'image/png');
    const {processedFile, imageWidth, imageHeight} =
        await processFile(originalFile, 100);

    assertNotEquals(processedFile, originalFile);
    assertLE(imageWidth!, 100);
    assertLE(imageHeight!, 100);
  });

  test(
      'does not bypass compression for unsupported C2PA mime type',
      async () => {
        const originalFile =
            await generateTestC2paImageFile(250, 250, 'image/bmp');
        const {processedFile, imageWidth, imageHeight} =
            await processFile(originalFile, 100);

        assertNotEquals(processedFile, originalFile);
        assertLE(imageWidth!, 100);
        assertLE(imageHeight!, 100);
      });

  test(
      'does not bypass compression when feature flag is disabled', async () => {
        loadTimeData.overrideValues({
          lensBypassCompressionForC2pa: false,
        });

        const originalFile =
            await generateTestC2paImageFile(250, 250, 'image/jpeg');
        const {processedFile, imageWidth, imageHeight} =
            await processFile(originalFile, 100);

        assertNotEquals(processedFile, originalFile);
        assertLE(imageWidth!, 100);
        assertLE(imageHeight!, 100);
      });
});
