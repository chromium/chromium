// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://compose/result_text.js';

import type {ComposeResultTextElement} from 'chrome-untrusted://compose/result_text.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

suite('ResultText', () => {
  let resultText: ComposeResultTextElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resultText = document.createElement('compose-result-text');

    // Make streaming work instantly.
    resultText.enableInstantStreamingForTesting();

    document.body.appendChild(resultText);
  });

  async function waitForStreaming() {
    // Text is streamed one word at a time. Wait long enough for all text to be
    // output.
    for (let i = 0; i < 10; i++) {
      await flushTasks();
    }
  }

  test('EmptyInitially', () => {
    assertEquals('', resultText.$.root.innerText);
  });

  test('FinalTextShown', () => {
    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: false,
      streamingEnabled: false,
    };
    assertEquals('Hi Mom, Happy Birthday!', resultText.$.root.innerText);
    assertEquals('', resultText.$.partialResultText.innerText);
    assertTrue(resultText.isOutputComplete);
    assertTrue(resultText.hasOutput);
  });

  test('StreamingFinalText', async () => {
    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: false,
      streamingEnabled: true,
    };

    assertEquals('', resultText.$.root.innerText);
    assertFalse(resultText.hasOutput);

    await waitForStreaming();

    assertEquals(
        'Hi Mom, Happy Birthday!', resultText.$.partialResultText.innerText);
    assertTrue(resultText.hasOutput);
    assertTrue(resultText.isOutputComplete);
  });


  test('StreamingFinalText', async () => {
    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: false,
      streamingEnabled: true,
    };

    assertEquals('', resultText.$.root.innerText);

    await waitForStreaming();

    assertEquals(
        'Hi Mom, Happy Birthday!', resultText.$.partialResultText.innerText);
  });

  test('StreamingPartialText', async () => {
    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: true,
      streamingEnabled: true,
    };

    assertEquals('', resultText.$.root.innerText);
    assertFalse(resultText.hasOutput);

    await waitForStreaming();

    assertEquals('Hi Mom, Happy', resultText.$.partialResultText.innerText);
    assertTrue(resultText.hasOutput);
    assertFalse(resultText.isOutputComplete);
  });

  test('StreamingPartialThenCompleteText', async () => {
    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: true,
      streamingEnabled: true,
    };

    await waitForStreaming();

    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: false,
      streamingEnabled: true,
    };

    await waitForStreaming();

    assertEquals(
        'Hi Mom, Happy Birthday!', resultText.$.partialResultText.innerText);
    assertTrue(resultText.hasOutput);
    assertTrue(resultText.isOutputComplete);
  });

  test('StreamingPartialThenCompleteWithError', async () => {
    resultText.textInput = {
      text: 'Hi Mom, Happy Birthday!',
      isPartial: true,
      streamingEnabled: true,
    };

    await waitForStreaming();

    resultText.textInput = {
      text: '',
      isPartial: false,
      streamingEnabled: true,
    };

    await waitForStreaming();

    assertEquals('', resultText.$.partialResultText.innerText);
    assertFalse(resultText.hasOutput);
    assertFalse(resultText.isOutputComplete);
  });
});
