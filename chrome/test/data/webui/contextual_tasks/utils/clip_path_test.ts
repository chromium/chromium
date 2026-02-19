// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getNonOccludedClipPath} from 'chrome://contextual-tasks/utils/clip_path.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

function assertClipPathEquals(expected: string, actual: string) {
  assertEquals(expected.replace(/\s/g, ''), actual.replace(/\s/g, ''));
}

suite('ClipPathTest', () => {
  test('returns empty string for null composebox bounds', () => {
    const result = getNonOccludedClipPath(null, [], 0);
    assertClipPathEquals('', result);
  });

  test('returns base polygon for no occluders', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 50,
      right: 150,
      width: 100,
      height: 100,
    };
    const result = getNonOccludedClipPath(composebox, [], 0);

    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '50px 200px, 150px 200px, 150px 100px, 50px 100px, 50px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('returns none for empty composebox', () => {
    const composebox = {
      top: 100,
      bottom: 100,
      left: 50,
      right: 50,
      width: 0,
      height: 0,
    };
    const result = getNonOccludedClipPath(composebox, [], 0);
    assertClipPathEquals('clip-path: none;', result);
  });

  test('clips top with header occluder', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder covers top 50px (100 to 150)
    const occluder = {
      top: 100,
      bottom: 150,
      left: 0,
      right: 100,
      width: 100,
      height: 50,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);

    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 100px 200px, 100px 150px, 0px 150px, 0px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('clips bottom with footer occluder', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder covers bottom 50px (150 to 200)
    const occluder = {
      top: 150,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 50,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);

    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 150px, ' +
        '0px 150px, 100px 150px, 100px 100px, 0px 100px, 0px 150px, ' +
        '0px 150px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('returns none for full vertical coverage', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    const occluder = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);
    assertClipPathEquals('clip-path: none;', result);
  });

  test('returns none for opposing occluders closing the hole', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    const header = {
      top: 100,
      bottom: 150,
      left: 0,
      right: 100,
      width: 100,
      height: 50,
    };
    const footer = {
      top: 150,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 50,
    };
    const result = getNonOccludedClipPath(composebox, [header, footer], 0);
    assertClipPathEquals('clip-path: none;', result);
  });

  test('ignores out of bounds full width occluder', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    const above = {
      top: 0,
      bottom: 50,
      left: 0,
      right: 100,
      width: 100,
      height: 50,
    };
    const below = {
      top: 250,
      bottom: 300,
      left: 0,
      right: 100,
      width: 100,
      height: 50,
    };
    const result = getNonOccludedClipPath(composebox, [above, below], 0);

    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 100px 200px, 100px 100px, 0px 100px, 0px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('padding upgrades to full width occluder', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Width is 80, but padding 10 makes it 100 (full width)
    const occluder = {
      top: 100,
      bottom: 150,
      left: 10,
      right: 90,
      width: 80,
      height: 50,
    };
    const padding = 10;
    const result = getNonOccludedClipPath(composebox, [occluder], padding);

    // Should act as header clipping top to 150 + 10 = 160
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 100px 200px, 100px 160px, 0px 160px, 0px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('padding closes the hole', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Almost full cover, but 10px gap. Padding closes it.
    const occluder = {
      top: 100,
      bottom: 190,
      left: 0,
      right: 100,
      width: 100,
      height: 90,
    };
    const padding = 10;
    const result = getNonOccludedClipPath(composebox, [occluder], padding);
    assertClipPathEquals('clip-path: none;', result);
  });

  test('handles no overlap', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder is to the right
    const occluder = {
      top: 100,
      bottom: 200,
      left: 150,
      right: 200,
      width: 50,
      height: 100,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);

    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 100px 200px, 100px 100px, 0px 100px, 0px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('handles full overlap of segment', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder 1 splits hole into left (0-40) and right (60-100).
    const occluder1 = {
      top: 120,
      bottom: 180,
      left: 40,
      right: 60,
      width: 20,
      height: 60,
    };
    // Occluder 2 covers left segment (0-40).
    const occluder2 = {
      top: 120,
      bottom: 180,
      left: 0,
      right: 40,
      width: 40,
      height: 60,
    };
    const result =
        getNonOccludedClipPath(composebox, [occluder1, occluder2], 0);

    // Expect only right segment (60-100)
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '60px 200px, 100px 200px, 100px 100px, 60px 100px, 60px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('handles left edge cut', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder cuts left side: 0 to 30
    const occluder = {
      top: 120,
      bottom: 180,
      left: 0,
      right: 30,
      width: 30,
      height: 60,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);

    // Expect segment 30-100
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '30px 200px, 100px 200px, 100px 100px, 30px 100px, 30px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('handles right edge cut', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder cuts right side: 70 to 100
    const occluder = {
      top: 120,
      bottom: 180,
      left: 70,
      right: 100,
      width: 30,
      height: 60,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);

    // Expect segment 0-70
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 70px 200px, 70px 100px, 0px 100px, 0px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('handles middle split', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Occluder in middle: 40 to 60
    const occluder = {
      top: 120,
      bottom: 180,
      left: 40,
      right: 60,
      width: 20,
      height: 60,
    };
    const result = getNonOccludedClipPath(composebox, [occluder], 0);

    // Expect segments 0-40 and 60-100
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 40px 200px, 40px 100px, 0px 100px, 0px 200px, ' +
        '60px 200px, 100px 200px, 100px 100px, 60px 100px, 60px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('handles multiple middle splits', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Three splits: 20-30, 50-60, 80-90
    const occluders = [
      {top: 120, bottom: 180, left: 20, right: 30, width: 10, height: 60},
      {top: 120, bottom: 180, left: 50, right: 60, width: 10, height: 60},
      {top: 120, bottom: 180, left: 80, right: 90, width: 10, height: 60},
    ];
    const result = getNonOccludedClipPath(composebox, occluders, 0);

    // Expect segments: 0-20, 30-50, 60-80, 90-100
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 20px 200px, 20px 100px, 0px 100px, 0px 200px, ' +
        '30px 200px, 50px 200px, 50px 100px, 30px 100px, 30px 200px, ' +
        '60px 200px, 80px 200px, 80px 100px, 60px 100px, 60px 200px, ' +
        '90px 200px, 100px 200px, 100px 100px, 90px 100px, 90px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('handles overlapping horizontal occluders', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    // Overlapping: 30-50 and 40-60. Union is 30-60.
    const occluders = [
      {top: 120, bottom: 180, left: 30, right: 50, width: 20, height: 60},
      {top: 120, bottom: 180, left: 40, right: 60, width: 20, height: 60},
    ];
    const result = getNonOccludedClipPath(composebox, occluders, 0);

    // Expect segments: 0-30 and 60-100
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 30px 200px, 30px 100px, 0px 100px, 0px 200px, ' +
        '60px 200px, 100px 200px, 100px 100px, 60px 100px, 60px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });

  test('sorts segments correctly', () => {
    const composebox = {
      top: 100,
      bottom: 200,
      left: 0,
      right: 100,
      width: 100,
      height: 100,
    };
    const occluders = [
      {top: 120, bottom: 180, left: 80, right: 90, width: 10, height: 60},
      {top: 120, bottom: 180, left: 10, right: 20, width: 10, height: 60},
    ];
    const result = getNonOccludedClipPath(composebox, occluders, 0);

    // Expect segments: 0-10, 20-80, 90-100
    const expected = 'clip-path: polygon(' +
        '0% 0%, 100% 0%, 100% 100%, 0% 100%, 0px 200px, ' +
        '0px 200px, 10px 200px, 10px 100px, 0px 100px, 0px 200px, ' +
        '20px 200px, 80px 200px, 80px 100px, 20px 100px, 20px 200px, ' +
        '90px 200px, 100px 200px, 100px 100px, 90px 100px, 90px 200px, ' +
        '0px 200px, 0% 100%);';
    assertClipPathEquals(expected, result);
  });
});
