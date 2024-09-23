// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

function getExpectedFontFamily(expectingSystemFont: boolean): string {
  if (!expectingSystemFont) {
    return 'Roboto';
  }

  const fontFamily =
      // <if expr="is_linux">
      '"DejaVu Sans"';
      // </if>
      // <if expr="is_macosx">
      'system-ui';
      // </if>
      // <if expr="is_win">
      '"Segoe UI"';
      // </if>
      // <if expr="chromeos_ash">
      'Roboto';
      // </if>
      // <if expr="chromeos_lacros">
      // TODO(crbug.com/40261940): Change to 'Roboto' once bug is fixed.
      'sans';
      // </if>
      // <if expr="is_fuchsia">
      // TODO(dpapad): WebUI tests are compiled on Fuchsia but don't seem to run
      // on any bot, so the value below does not matter, it just makes the code
      // syntactically valid. Figure out whether the tests should be run, or
      // excluded from compilation on Fuchsia.
      'unknown';
      // </if>

  return fontFamily;
}

// Asserts that a CSS rule specifying font-family with the expected value
// exists.
function assertFontFamilyRule(
    link: HTMLLinkElement, expectingSystemFont: boolean) {
  assertTrue(!!link.sheet);
  const styleRules =
      Array.from(link.sheet.cssRules).filter(r => r instanceof CSSStyleRule) as
      CSSStyleRule[];
  assertTrue(styleRules.length > 0);

  const fontFamily = styleRules[0]!.style.getPropertyValue('font-family');
  const expectedFontFamily = getExpectedFontFamily(expectingSystemFont);
  assertTrue(
      fontFamily.startsWith(expectedFontFamily),
      `Found: '${fontFamily.toString()}'`);
}

// Asserts that a 'div' inherits the expected font-family value.
function assertFontFamilyApplied(expectingSystemFont: boolean) {
  const div = document.createElement('div');
  div.textContent = 'Dummy text';
  document.body.appendChild(div);

  const fontFamily = div.computedStyleMap().get('font-family');
  assertTrue(!!fontFamily);
  const expectedFontFamily = getExpectedFontFamily(expectingSystemFont);
  assertTrue(
      fontFamily.toString().startsWith(expectedFontFamily),
      `Found: '${fontFamily.toString()}'`);
}


function testFontFamily(
    cssFile: string, expectingSystemFont: boolean): Promise<void> {
  const resolver = new PromiseResolver<void>();

  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = cssFile;

  link.onload = function() {
    assertFontFamilyRule(link, expectingSystemFont);
    assertFontFamilyApplied(expectingSystemFont);
    resolver.resolve();
  };
  document.body.appendChild(link);

  return resolver.promise;
}

suite('TextDefaults', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('text_defaults.css', function() {
    return testFontFamily(
        'chrome://resources/css/text_defaults.css',
        true /*expectingSystemFont*/);
  });

  test('text_defaults_md.css', function() {
    let expectingSystemFont = true;
    // <if expr="is_linux">
    expectingSystemFont = false;
    // </if>

    return testFontFamily(
        'chrome://resources/css/text_defaults_md.css', expectingSystemFont);
  });
});
