// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-code-section. */
import 'chrome://extensions/extensions.js';

import type {CodeSectionElement} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ExtensionCodeSectionTest', function() {
  let codeSection: CodeSectionElement;

  const couldNotDisplayCode: string = 'No code here';

  // Initialize an extension item before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    codeSection = document.createElement('extensions-code-section');
    codeSection.couldNotDisplayCode = couldNotDisplayCode;
    document.body.appendChild(codeSection);
  });

  test('Layout', async () => {
    const code: chrome.developerPrivate.RequestFileSourceResponse = {
      beforeHighlight: 'this part before the highlight\nAnd this too\n',
      highlight: 'highlight this part\n',
      afterHighlight: 'this part after the highlight',
      message: 'Highlight message',
      title: '',
    };

    const testIsVisible = isChildVisible.bind(null, codeSection);
    assertFalse(!!codeSection.code);
    assertTrue(
        codeSection.shadowRoot!.querySelector<HTMLElement>(
                                   '#scroll-container')!.hidden);
    assertFalse(testIsVisible('#main'));
    assertTrue(testIsVisible('#no-code'));

    codeSection.code = code;
    codeSection.isActive = true;
    await microtasksFinished();

    assertTrue(testIsVisible('#main'));
    assertFalse(testIsVisible('#no-code'));

    const codeSections =
        codeSection.shadowRoot!.querySelectorAll('#source > span > *');

    assertEquals(code.beforeHighlight, codeSections[0]!.textContent);
    assertEquals(code.highlight, codeSections[1]!.textContent);
    assertEquals(code.afterHighlight, codeSections[2]!.textContent);

    assertEquals(
        '1\n2\n3\n4',
        codeSection.shadowRoot!
            .querySelector<HTMLElement>(
                '#line-numbers span')!.textContent!.trim());
  });

  test('LongSource', async () => {
    let lineNums;

    function setCodeContent(beforeLineCount: number, afterLineCount: number):
        chrome.developerPrivate.RequestFileSourceResponse {
      const code: chrome.developerPrivate.RequestFileSourceResponse = {
        beforeHighlight: '',
        highlight: 'highlight',
        afterHighlight: '',
        message: 'Highlight message',
        title: '',
      };
      for (let i = 0; i < beforeLineCount; i++) {
        code.beforeHighlight += 'a\n';
      }
      for (let i = 0; i < afterLineCount; i++) {
        code.afterHighlight += 'a\n';
      }
      return code;
    }

    codeSection.code = setCodeContent(0, 2000);
    await microtasksFinished();

    lineNums =
        codeSection.shadowRoot!
            .querySelector<HTMLElement>('#line-numbers span')!.textContent!;
    // Length should be 1000 +- 1.
    assertTrue(lineNums.split('\n').length >= 999);
    assertTrue(lineNums.split('\n').length <= 1001);
    assertTrue(!!lineNums.match(/^1\n/));
    assertTrue(!!lineNums.match(/1000/));
    assertFalse(!!lineNums.match(/1001/));
    assertTrue(codeSection.shadowRoot!
                   .querySelector<HTMLElement>(
                       '#line-numbers .more-code.before')!.hidden);
    assertFalse(codeSection.shadowRoot!
                    .querySelector<HTMLElement>(
                        '#line-numbers .more-code.after')!.hidden);

    codeSection.code = setCodeContent(1000, 1000);
    await microtasksFinished();

    lineNums =
        codeSection.shadowRoot!
            .querySelector<HTMLElement>('#line-numbers span')!.textContent!;
    // Length should be 1000 +- 1.
    assertTrue(lineNums.split('\n').length >= 999);
    assertTrue(lineNums.split('\n').length <= 1001);
    assertFalse(!!lineNums.match(/^1\n/));
    assertTrue(!!lineNums.match(/1000/));
    assertFalse(!!lineNums.match(/1999/));
    assertFalse(codeSection.shadowRoot!
                    .querySelector<HTMLElement>(
                        '#line-numbers .more-code.before')!.hidden);
    assertFalse(codeSection.shadowRoot!
                    .querySelector<HTMLElement>(
                        '#line-numbers .more-code.after')!.hidden);

    codeSection.code = setCodeContent(2000, 0);
    await microtasksFinished();

    lineNums =
        codeSection.shadowRoot!
            .querySelector<HTMLElement>('#line-numbers span')!.textContent!;
    // Length should be 1000 +- 1.
    assertTrue(lineNums.split('\n').length >= 999);
    assertTrue(lineNums.split('\n').length <= 1001);
    assertFalse(!!lineNums.match(/^1\n/));
    assertTrue(!!lineNums.match(/1002/));
    assertTrue(!!lineNums.match(/2000/));
    assertFalse(codeSection.shadowRoot!
                    .querySelector<HTMLElement>(
                        '#line-numbers .more-code.before')!.hidden);
    assertTrue(codeSection.shadowRoot!
                   .querySelector<HTMLElement>(
                       '#line-numbers .more-code.after')!.hidden);
  });
});
