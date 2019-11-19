// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-code-section. */
import 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isVisible} from '../test_util.m.js';

window.extension_code_section_tests = {};
extension_code_section_tests.suiteName = 'ExtensionCodeSectionTest';
/** @enum {string} */
extension_code_section_tests.TestNames = {
  Layout: 'layout',
  LongSource: 'long source',
};

suite(extension_code_section_tests.suiteName, function() {
  /** @type {ExtensionsCodeSectionElement} */
  let codeSection;

  const couldNotDisplayCode = 'No code here';

  // Initialize an extension item before each test.
  setup(function() {
    PolymerTest.clearBody();
    codeSection = document.createElement('extensions-code-section');
    codeSection.couldNotDisplayCode = couldNotDisplayCode;
    document.body.appendChild(codeSection);
  });

  test(assert(extension_code_section_tests.TestNames.Layout), function() {
    /** @type {chrome.developerPrivate.RequestFileSourceResponse} */
    const code = {
      beforeHighlight: 'this part before the highlight\nAnd this too\n',
      highlight: 'highlight this part\n',
      afterHighlight: 'this part after the highlight',
      message: 'Highlight message',
    };

    const testIsVisible = isVisible.bind(null, codeSection);
    expectFalse(!!codeSection.code);
    expectTrue(codeSection.$$('#scroll-container').hidden);
    expectFalse(testIsVisible('#main'));
    expectTrue(testIsVisible('#no-code'));

    codeSection.code = code;
    codeSection.isActive = true;
    expectTrue(testIsVisible('#main'));
    expectFalse(testIsVisible('#no-code'));

    let codeSections =
        codeSection.shadowRoot.querySelectorAll('#source span span');

    expectEquals(code.beforeHighlight, codeSections[0].textContent);
    expectEquals(code.highlight, codeSections[1].textContent);
    expectEquals(code.afterHighlight, codeSections[2].textContent);

    expectEquals(
        '1\n2\n3\n4', codeSection.$$('#line-numbers span').textContent.trim());
  });

  test(assert(extension_code_section_tests.TestNames.LongSource), function() {
    /** @type {chrome.developerPrivate.RequestFileSourceResponse} */
    let code;
    let lineNums;

    function setCodeContent(beforeLineCount, afterLineCount) {
      code = {
        beforeHighlight: '',
        highlight: 'highlight',
        afterHighlight: '',
        message: 'Highlight message',
      };
      for (let i = 0; i < beforeLineCount; i++) {
        code.beforeHighlight += 'a\n';
      }
      for (let i = 0; i < afterLineCount; i++) {
        code.afterHighlight += 'a\n';
      }
    }

    setCodeContent(0, 2000);
    codeSection.code = code;
    lineNums = codeSection.$$('#line-numbers span').textContent;
    // Length should be 1000 +- 1.
    expectTrue(lineNums.split('\n').length >= 999);
    expectTrue(lineNums.split('\n').length <= 1001);
    expectTrue(!!lineNums.match(/^1\n/));
    expectTrue(!!lineNums.match(/1000/));
    expectFalse(!!lineNums.match(/1001/));
    expectTrue(codeSection.$$('#line-numbers .more-code.before').hidden);
    expectFalse(codeSection.$$('#line-numbers .more-code.after').hidden);

    setCodeContent(1000, 1000);
    codeSection.code = code;
    lineNums = codeSection.$$('#line-numbers span').textContent;
    // Length should be 1000 +- 1.
    expectTrue(lineNums.split('\n').length >= 999);
    expectTrue(lineNums.split('\n').length <= 1001);
    expectFalse(!!lineNums.match(/^1\n/));
    expectTrue(!!lineNums.match(/1000/));
    expectFalse(!!lineNums.match(/1999/));
    expectFalse(codeSection.$$('#line-numbers .more-code.before').hidden);
    expectFalse(codeSection.$$('#line-numbers .more-code.after').hidden);

    setCodeContent(2000, 0);
    codeSection.code = code;
    lineNums = codeSection.$$('#line-numbers span').textContent;
    // Length should be 1000 +- 1.
    expectTrue(lineNums.split('\n').length >= 999);
    expectTrue(lineNums.split('\n').length <= 1001);
    expectFalse(!!lineNums.match(/^1\n/));
    expectTrue(!!lineNums.match(/1002/));
    expectTrue(!!lineNums.match(/2000/));
    expectFalse(codeSection.$$('#line-numbers .more-code.before').hidden);
    expectTrue(codeSection.$$('#line-numbers .more-code.after').hidden);
  });
});
