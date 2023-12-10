// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';

import {CrFeedbackButtonsElement, CrFeedbackOption} from 'chrome://resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('CrFeedbackButtonsTest', () => {
  let element: CrFeedbackButtonsElement;

  suiteSetup(() => {
    loadTimeData.resetForTesting({
      thumbsDown: 'thumbs down',
      thumbsUp: 'thumbs up',
    });
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-feedback-buttons');
    document.body.appendChild(element);
    await flushTasks();
  });

  test('SetsLabels', () => {
    assertEquals('thumbs up', element.$.thumbsUp.ariaLabel);
    assertEquals('thumbs down', element.$.thumbsDown.ariaLabel);
  });

  test('TogglesIconState', () => {
    assertEquals('cr:thumbs-up', element.$.thumbsUp.ironIcon);
    assertEquals('false', element.$.thumbsUp.ariaPressed);
    assertEquals('cr:thumbs-down', element.$.thumbsDown.ironIcon);
    assertEquals('false', element.$.thumbsDown.ariaPressed);

    element.$.thumbsUp.click();
    assertEquals('cr:thumbs-up-filled', element.$.thumbsUp.ironIcon);
    assertEquals('true', element.$.thumbsUp.ariaPressed);
    assertEquals('cr:thumbs-down', element.$.thumbsDown.ironIcon);
    assertEquals('false', element.$.thumbsDown.ariaPressed);

    element.$.thumbsUp.click();
    assertEquals('cr:thumbs-up', element.$.thumbsUp.ironIcon);
    assertEquals('false', element.$.thumbsUp.ariaPressed);
    assertEquals('cr:thumbs-down', element.$.thumbsDown.ironIcon);
    assertEquals('false', element.$.thumbsDown.ariaPressed);

    element.$.thumbsDown.click();
    assertEquals('cr:thumbs-up', element.$.thumbsUp.ironIcon);
    assertEquals('false', element.$.thumbsUp.ariaPressed);
    assertEquals('cr:thumbs-down-filled', element.$.thumbsDown.ironIcon);
    assertEquals('true', element.$.thumbsDown.ariaPressed);

    element.$.thumbsDown.click();
    assertEquals('cr:thumbs-up', element.$.thumbsUp.ironIcon);
    assertEquals('false', element.$.thumbsUp.ariaPressed);
    assertEquals('cr:thumbs-down', element.$.thumbsDown.ironIcon);
    assertEquals('false', element.$.thumbsDown.ariaPressed);
  });

  test('SendsEvent', async () => {
    const thumbsUpEvent = eventToPromise('selected-option-changed', element);
    element.$.thumbsUp.click();
    const thumbsUpEventArgs = await thumbsUpEvent;
    assertEquals(CrFeedbackOption.THUMBS_UP, thumbsUpEventArgs.detail.value);

    const thumbsDownEvent = eventToPromise('selected-option-changed', element);
    element.$.thumbsDown.click();
    const thumbsDownEventArgs = await thumbsDownEvent;
    assertEquals(
        CrFeedbackOption.THUMBS_DOWN, thumbsDownEventArgs.detail.value);

    const noThumbsEvent = eventToPromise('selected-option-changed', element);
    element.$.thumbsDown.click();
    const noThumbsEventArgs = await noThumbsEvent;
    assertEquals(CrFeedbackOption.UNSPECIFIED, noThumbsEventArgs.detail.value);
  });

  test('AcceptsSelectedOptionBinding', () => {
    element.selectedOption = CrFeedbackOption.THUMBS_UP;
    assertEquals('cr:thumbs-up-filled', element.$.thumbsUp.ironIcon);
    assertEquals('true', element.$.thumbsUp.ariaPressed);
  });
});
