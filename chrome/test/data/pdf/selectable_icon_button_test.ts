// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertLabels} from './test_util.js';

chrome.test.runTests([
  async function testButtonProperties() {
    const button = document.createElement('selectable-icon-button');
    button.icon = 'pdf-ink:ink-pen';
    button.label = 'Pen';
    document.body.innerHTML = '';
    document.body.appendChild(button);
    await microtasksFinished();

    // Correctly overrides noRipple.
    chrome.test.assertTrue(button.noRipple);
    // Correctly passes the icon through to cr-icon-button and sets tooltip and
    // aria-label with |label|.
    chrome.test.assertEq(
        'pdf-ink:ink-pen', button.$.button.getAttribute('iron-icon'));
    assertLabels(button.$.button, 'Pen');

    // Test changing properties works.
    button.icon = 'pdf-ink:ink-eraser';
    button.label = 'Eraser';
    await microtasksFinished();
    chrome.test.assertEq(
        'pdf-ink:ink-eraser', button.$.button.getAttribute('iron-icon'));
    assertLabels(button.$.button, 'Eraser');
    chrome.test.succeed();
  },
]);
