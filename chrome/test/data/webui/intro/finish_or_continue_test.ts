// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/finish_or_continue/app.js';

import type {FinishOrContinueAppElement} from 'chrome://intro/finish_or_continue/app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isWindows} from 'chrome://resources/js/platform.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('FinishOrContinueTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Reset URL to default before each test to ensure isolation.
    const url = new URL(window.location.href);
    url.searchParams.delete('showcase');
    window.history.replaceState({}, '', url.toString());
  });

  async function createElement(): Promise<FinishOrContinueAppElement> {
    const element = document.createElement('finish-or-continue-app');
    document.body.appendChild(element);
    await microtasksFinished();
    return element;
  }

  test('ButtonsExist', async function() {
    const testElement = await createElement();
    assertTrue(!!testElement.$.continueEducationButton);
    assertTrue(!!testElement.$.startBrowsingButton);
    assertTrue(
        testElement.$.startBrowsingButton.classList.contains('action-button'));
  });

  test('ButtonsOrder', async function() {
    const testElement = await createElement();
    const buttonContainer = testElement.$.buttonContainer;
    assertTrue(!!buttonContainer);

    const buttons = buttonContainer.querySelectorAll('cr-button');
    assertEquals(2, buttons.length);

    if (isWindows) {
      assertEquals(testElement.$.startBrowsingButton, buttons[0]);
      assertEquals(testElement.$.continueEducationButton, buttons[1]);
    } else {
      assertEquals(testElement.$.continueEducationButton, buttons[0]);
      assertEquals(testElement.$.startBrowsingButton, buttons[1]);
    }
  });

  test('SeeWhatsNewButtonLabel_NoShowcaseParam', async function() {
    const testElement = await createElement();

    assertEquals(
        loadTimeData.getString('seeWhatsNewButtonLabel'),
        testElement.$.continueEducationButton.textContent.trim());
  });

  test('SeeWhatsNewButtonLabel_ShowcaseFalse', async function() {
    const url = new URL(window.location.href);
    url.searchParams.set('showcase', 'false');
    window.history.replaceState({}, '', url.toString());

    const testElement = await createElement();

    assertEquals(
        loadTimeData.getString('seeWhatsNewButtonLabel'),
        testElement.$.continueEducationButton.textContent.trim());
  });

  test('ContinueButtonLabel_ShowcaseTrue', async function() {
    const url = new URL(window.location.href);
    url.searchParams.set('showcase', 'true');
    window.history.replaceState({}, '', url.toString());

    const testElement = await createElement();

    assertEquals(
        loadTimeData.getString('seeMoreTipsButtonLabel'),
        testElement.$.continueEducationButton.textContent.trim());
  });
});
