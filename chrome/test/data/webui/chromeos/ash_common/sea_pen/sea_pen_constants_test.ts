// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {parseTemplateText} from 'chrome://resources/ash/common/sea_pen/constants.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('parseTemplateTextTest', function() {
  test('empty template text', function() {
    const templateTokens: string[] = parseTemplateText('');
    assertEquals(0, templateTokens.length);
  });

  test('template text without chip token', function() {
    const templateTokens: string[] =
        parseTemplateText('A radiant pink rose in bloom');
    assertEquals(1, templateTokens.length);
    assertEquals('A radiant pink rose in bloom', templateTokens[0]);
  });

  test('template text with one chip token', function() {
    const templateTokens: string[] =
        parseTemplateText('A radiant <pink> rose in bloom');
    assertEquals(3, templateTokens.length);
    assertEquals('A radiant', templateTokens[0]);
    assertEquals('<pink>', templateTokens[1]);
    assertEquals('rose in bloom', templateTokens[2]);
  });

  test('template text with two adjacent chip tokens', function() {
    const templateTokens: string[] =
        parseTemplateText('A radiant <pink> <rose> in bloom');
    assertEquals(4, templateTokens.length);
    assertEquals('A radiant', templateTokens[0]);
    assertEquals('<pink>', templateTokens[1]);
    assertEquals('<rose>', templateTokens[2]);
    assertEquals('in bloom', templateTokens[3]);
  });
});
