// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {getBoldedDescription} from 'chrome://shortcut-customization/js/search/search_result_bolding.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('searchResultBoldingTest', function() {
  test('Tokenize and match result text to query text', () => {
    assertEquals(
        '<b>Open</b> new <b>tab</b>',
        getBoldedDescription('Open new tab', 'Open tab').toString());
  });

  test('Bold result text to matching query', () => {
    assertEquals(
        'Op<b>e</b>n n<b>e</b>w tab in brows<b>e</b>r',
        getBoldedDescription('Open new tab in browser', 'e').toString());
  });

  test('Bold result including ignored characters', () => {
    let description = 'Turn on Wi-Fi';
    assertEquals(
        'Turn on <b>Wi-F</b>i',
        getBoldedDescription(description, 'wif').toString());
    assertEquals(
        'Turn on <b>Wi-F</b>i',
        getBoldedDescription(description, 'wi f').toString());
    assertEquals(
        'Turn on <b>Wi-F</b>i',
        getBoldedDescription(description, 'wi-f').toString());

    description = 'Enable touchpad tap-to-click';
    assertEquals(
        'Enable touchpad <b>tap-to-cli</b>ck',
        getBoldedDescription(description, 'tap to cli').toString());
    assertEquals(
        'Enable touchpad <b>tap-to-cli</b>ck',
        getBoldedDescription(description, 'taptocli').toString());
    assertEquals(
        'Enable touchpad <b>tap-to-cli</b>ck',
        getBoldedDescription(description, 'tap-to-cli').toString());
    assertEquals(
        'Enable touchpad <b>tap-to-cli</b>ck',
        getBoldedDescription(description, 'tap top cli').toString());

    assertEquals(
        'w<b>xy</b>z <b>Tap-To</b>-Click',
        getBoldedDescription('wxyz Tap-To-Click', 'tap toxy cli').toString());

    assertEquals(
        '<b>Tap</b>-to-click <b>Ti</b>ps <b>Ti</b>tle',
        getBoldedDescription('Tap-to-click Tips Title', 'tap ti').toString());
  });

  test('Test bolding of accented characters', () => {
    const description = 'Crème Brûlée';
    assertEquals(
        'Cr<b>è</b>me Br<b>û</b>l<b>é</b>e',
        getBoldedDescription(description, 'E U').toString());
    assertEquals(
        '<b>Crème</b> Brûlée',
        getBoldedDescription(description, 'creme').toString());
    assertEquals(
        'Crè<b>me</b> <b>Brû</b>lée',
        getBoldedDescription(description, 'me bru').toString());
  });

  test('Test no spaces nor characters that have upper/lower case', () => {
    const description = 'キーボード設定---';  // Keyboard settings
    assertEquals(
        '<b>キ</b><b>ー</b>ボ<b>ー</b>ド<b>設</b>定---',
        getBoldedDescription(description, 'キー設').toString());
    assertEquals(
        'キーボード<b>設</b>定---',
        getBoldedDescription(description, '設').toString());
  });

  test('Test RTL languages', () => {
    const description = 'افتح علامة تبويب جديدة';  // Open new tab
    assertEquals(
        'افتح علامة <b>تبويب</b> جديدة',
        getBoldedDescription(description, 'تبويب').toString());
  });

  test('Test blankspace types in result maintained', async () => {
    const description = 'Turn&nbsp;on  &nbsp;Wi-Fi ';
    assertEquals(
        'Turn&nbsp;on  &nbsp;<b>Wi-F</b>i ',
        getBoldedDescription(description, 'wif').toString());
  });
});
