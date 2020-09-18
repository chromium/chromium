// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function detectedLanguageSetOnFirst() {
    var first = rootNode.children[0].children[0];
    assertEq('staticText', first.role);
    assertEq('fr', first.language, 'document manually declares lang="fr"');
    assertEq('en', first.detectedLanguage,
      'detected language should be English');
    chrome.test.succeed();
  },

  function detectedLanguageSetOnSecond() {
    var second = rootNode.children[1].children[0];
    assertEq('staticText', second.role);
    assertEq('en', second.language, 'document manually declares lang="en"');
    assertEq('fr', second.detectedLanguage,
      'detected language should be French');
    chrome.test.succeed();
  },

  function detectedLanguageForInvalidAttribute() {
    var item = rootNode.children[2].children[0];
    var langAnnotation =
      item.languageAnnotationForStringAttribute('invalid attribute');
    assertEq(0, langAnnotation.length);
    chrome.test.succeed();
  },

  function detectedLanguageCharacter() {
    var item = rootNode.children[2].children[0];
    var langAnnotation = item.languageAnnotationForStringAttribute('name');
    var name = item.name;
    assertEq(1, langAnnotation.length);
    assertEq('el', langAnnotation[0].language);
    var startIndex = langAnnotation[0].startIndex;
    var endIndex = langAnnotation[0].endIndex;
    assertEq(name,buildOutputString(name,startIndex,endIndex));
    chrome.test.succeed();
  },

  function detectedLanguageMultiple() {
    var item = rootNode.children[3].children[0];
    var langAnnotation = item.languageAnnotationForStringAttribute('name');
    var name = item.name;
    assertEq(3, langAnnotation.length);
    assertEq('en', langAnnotation[0].language);
    assertEq('ja', langAnnotation[1].language);
    assertEq('en', langAnnotation[2].language);

    // Build substrings.
    var actualSubstrings = [];
    for (var q = 0; q < langAnnotation.length; ++q) {
      var output = '';
      var startIndex = langAnnotation[q].startIndex;
      var endIndex = langAnnotation[q].endIndex;
      actualSubstrings.push(buildOutputString(name,startIndex,endIndex));
    }

    // Can assertEq on the two English strings.
    assertEq('Hello everyone, it\'s a pleasure to meet you. ',
      actualSubstrings[0]);
    assertEq('Let\'s include more English to ensure that we\'re splitting ' +
      'indices correctly.', actualSubstrings[2]);
    // Compare unicode code points to ensure correct symbols for Japanese
    // substring.
    // correctJapaneseCodePoints contains the codePoints for the string:
    // どうぞよろしくお願いします.
    var correctJapaneseCodePoints = [12393, 12358, 12382, 12424, 12429, 12375,
      12367, 12362, 39000, 12356, 12375, 12414, 12377, 46];
    var japaneseSubstring = actualSubstrings[1];
    var japaneseSubstringSymbolArray = [...japaneseSubstring];
    assertEq(japaneseSubstringSymbolArray.length,
      correctJapaneseCodePoints.length);
    for (var i = 0; i < japaneseSubstringSymbolArray.length; ++i) {
      var codePoint = japaneseSubstring.codePointAt(i);
      assertEq(correctJapaneseCodePoints[i], codePoint);
    }
    chrome.test.succeed();
  },
  // This function ensures correct behavior when building substrings that
  // contain unicode surrogate pairs.
  function testBuildOutputStringOnSurrogatePair() {
    // containsSurrogatePair = '𝛀 is bold omega'.
    var containsSurrogatePair = '\u{1D6C0} is bold omega';
    var symbolArray = [...containsSurrogatePair];
    // To humans, the length of 𝛀 is 1, but to the computer, it actually has a
    // length of 2.
    // Bold omega is a single unicode code point which is 2 code units in Utf16.
    // The first assertion below confirms this, as the string.length method
    // is not unicode-aware.
    assertEq(16,containsSurrogatePair.length);
    // However, the second assertion is unicode aware, which is the length most
    // people would expect given the above string.
    assertEq(15,symbolArray.length);

    // Extract only the omega from the original string.
    // Notice that when using the string.substring method, we need to pass in
    // endIndex = 2 to extract the 𝛀.
    // However, the buildOutputString function is unicode-aware, so to
    // extract the 𝛀, we can set endIndex = 1, instead of 2.
    var omega = containsSurrogatePair.substring(0,2);
    assertEq('1d6c0',omega.codePointAt(0).toString(16));
    omega = buildOutputString(containsSurrogatePair,0,1);
    assertEq('1d6c0',omega.codePointAt(0).toString(16));
    chrome.test.succeed();
  }
];

// Returns unicode-aware substring of text from startIndex to endIndex.
function buildOutputString(text,startIndex,endIndex) {
  var result = '';
  var textSymbolArray = [...text];
  for (var i = startIndex; i < endIndex; ++i) {
    result += textSymbolArray[i];
  }
  return result;
}

setUpAndRunTests(allTests, 'detected_language.html');
