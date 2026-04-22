// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [
  function detectedLanguageSetOnFirst() {
    const first = rootNode.children[0].children[0];
    assertEq('staticText', first.role);
    assertEq('fr', first.language, 'document manually declares lang="fr"');
    assertEq(
        'en', first.detectedLanguage, 'detected language should be English');
    chrome.test.succeed();
  },

  function detectedLanguageSetOnSecond() {
    const second = rootNode.children[1].children[0];
    assertEq('staticText', second.role);
    assertEq('en', second.language, 'document manually declares lang="en"');
    assertEq(
        'fr', second.detectedLanguage, 'detected language should be French');
    chrome.test.succeed();
  },

  // This function ensures correct behavior when building substrings that
  // contain unicode surrogate pairs.
  function testBuildOutputStringOnSurrogatePair() {
    // containsSurrogatePair = '𝛀 is bold omega'.
    const containsSurrogatePair = '\u{1D6C0} is bold omega';
    const symbolArray = [...containsSurrogatePair];
    // To humans, the length of 𝛀 is 1, but to the computer, it actually has a
    // length of 2.
    // Bold omega is a single unicode code point which is 2 code units in Utf16.
    // The first assertion below confirms this, as the string.length method
    // is not unicode-aware.
    assertEq(16, containsSurrogatePair.length);
    // However, the second assertion is unicode aware, which is the length most
    // people would expect given the above string.
    assertEq(15, symbolArray.length);

    // Extract only the omega from the original string.
    // Notice that when using the string.substring method, we need to pass in
    // endIndex = 2 to extract the 𝛀.
    // However, the buildOutputString function is unicode-aware, so to
    // extract the 𝛀, we can set endIndex = 1, instead of 2.
    let omega = containsSurrogatePair.substring(0, 2);
    assertEq('1d6c0', omega.codePointAt(0).toString(16));
    omega = buildOutputString(containsSurrogatePair, 0, 1);
    assertEq('1d6c0', omega.codePointAt(0).toString(16));
    chrome.test.succeed();
  },
];

// Returns unicode-aware substring of text from startIndex to endIndex.
function buildOutputString(text, startIndex, endIndex) {
  let result = '';
  const textSymbolArray = [...text];
  for (let i = startIndex; i < endIndex; ++i) {
    result += textSymbolArray[i];
  }
  return result;
}

setUpAndRunTabsTests(allTests, 'detected_language.html');
