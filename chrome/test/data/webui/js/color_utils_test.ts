// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexColorToSkColor, skColorToHexColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('ColorUtilsTest', () => {
  test('Can convert simple SkColors to rgba strings', () => {
    assertEquals(skColorToRgba({value: 0xffff0000}), 'rgba(255, 0, 0, 1.00)');
    assertEquals(skColorToRgba({value: 0xff00ff00}), 'rgba(0, 255, 0, 1.00)');
    assertEquals(skColorToRgba({value: 0xff0000ff}), 'rgba(0, 0, 255, 1.00)');
    assertEquals(
        skColorToRgba({value: 0xffffffff}), 'rgba(255, 255, 255, 1.00)');
    assertEquals(skColorToRgba({value: 0xff000000}), 'rgba(0, 0, 0, 1.00)');
  });

  test('Can convert complex SkColors to rgba strings', () => {
    assertEquals(
        skColorToRgba({value: 0xC0a11f8f}), 'rgba(161, 31, 143, 0.75)');
    assertEquals(skColorToRgba({value: 0x802b6335}), 'rgba(43, 99, 53, 0.50)');
    assertEquals(
        skColorToRgba({value: 0x40e3d2c1}), 'rgba(227, 210, 193, 0.25)');
  });

  test('Can convert simple SkColors to hex strings', () => {
    assertEquals(skColorToHexColor({value: 0xffff0000}), '#ff0000');
    assertEquals(skColorToHexColor({value: 0xff00ff00}), '#00ff00');
    assertEquals(skColorToHexColor({value: 0xff0000ff}), '#0000ff');
    assertEquals(skColorToHexColor({value: 0xffffffff}), '#ffffff');
    assertEquals(skColorToHexColor({value: 0xff000000}), '#000000');
  });

  test('Can convert complex SkColors to hex strings', () => {
    assertEquals(skColorToHexColor({value: 0xC0a11f8f}), '#a11f8f');
    assertEquals(skColorToHexColor({value: 0x802b6335}), '#2b6335');
    assertEquals(skColorToHexColor({value: 0x40e3d2c1}), '#e3d2c1');
  });

  test('Can convert simple hex strings to SkColors', () => {
    assertDeepEquals(hexColorToSkColor('#ff0000'), {value: 0xffff0000});
    assertDeepEquals(hexColorToSkColor('#00ff00'), {value: 0xff00ff00});
    assertDeepEquals(hexColorToSkColor('#0000ff'), {value: 0xff0000ff});
    assertDeepEquals(hexColorToSkColor('#ffffff'), {value: 0xffffffff});
    assertDeepEquals(hexColorToSkColor('#000000'), {value: 0xff000000});
  });

  test('Can convert complex hex strings to SkColors', () => {
    assertDeepEquals(hexColorToSkColor('#a11f8f'), {value: 0xffa11f8f});
    assertDeepEquals(hexColorToSkColor('#2b6335'), {value: 0xff2b6335});
    assertDeepEquals(hexColorToSkColor('#e3d2c1'), {value: 0xffe3d2c1});
  });

  test('Cannot convert malformed hex strings to SkColors', () => {
    assertDeepEquals(hexColorToSkColor('#fffffr'), {value: 0});
    assertDeepEquals(hexColorToSkColor('#ffffff0'), {value: 0});
    assertDeepEquals(hexColorToSkColor('ffffff'), {value: 0});
  });
});
