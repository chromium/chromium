// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testFlags(streamId, flags) {
  // These stream IDs must match the values in the
  // DictationPrivateApiStartStreamFlagsTest parameterized test instantiation.
  const STREAM_ID_EXPECTING_EVAL_MODE = 1001;
  const STREAM_ID_EXPECTING_WEB_SPEECH_API_BACKEND = 1002;

  chrome.test.assertTrue(flags !== undefined);
  if (streamId === STREAM_ID_EXPECTING_EVAL_MODE) {
    chrome.test.assertEq(true, flags.evalMode);
    chrome.test.assertEq(false, flags.webSpeechApiBackend);
  } else if (streamId === STREAM_ID_EXPECTING_WEB_SPEECH_API_BACKEND) {
    chrome.test.assertEq(false, flags.evalMode);
    chrome.test.assertEq(true, flags.webSpeechApiBackend);
  } else {
    chrome.test.assertEq(false, flags.evalMode);
    chrome.test.assertEq(false, flags.webSpeechApiBackend);
  }
}

if (chrome.dictationPrivate === undefined) {
  console.error('chrome.dictationPrivate is undefined');
  chrome.test.sendMessage('failed');
} else {
  chrome.dictationPrivate.onStartStream.addListener(async (details) => {
    const {streamId, context, flags} = details;
    const {annotatedPageContent, innerText, editableContent} = context;

    testFlags(streamId, flags);

    chrome.test.assertTrue(annotatedPageContent instanceof ArrayBuffer);
    const view = new Uint8Array(annotatedPageContent);
    chrome.test.assertEq(3, view.length);
    chrome.test.assertEq(1, view[0]);
    chrome.test.assertEq(2, view[1]);
    chrome.test.assertEq(3, view[2]);
    chrome.test.assertEq('Foo Bar', innerText);
    chrome.test.assertEq('Existing content', editableContent);

    chrome.test.assertTrue(
        typeof chrome.dictationPrivate.updateTranscription === 'function');
    chrome.test.assertTrue(
        typeof chrome.dictationPrivate.setStreamState === 'function');

    try {
      await chrome.dictationPrivate.setStreamState({
        streamId,
        state: chrome.dictationPrivate.StreamState.TRANSCRIBING,
      });
      await chrome.dictationPrivate.updateTranscription({
        streamId,
        type: chrome.dictationPrivate.TranscriptionType.PARTIAL,
        data: 'Hello',
      });
      await chrome.dictationPrivate.updateTranscription({
        streamId,
        type: chrome.dictationPrivate.TranscriptionType.FINAL,
        data: 'Hello world',
      });
      await chrome.dictationPrivate.setStreamState({
        streamId,
        state: chrome.dictationPrivate.StreamState.COMPLETE,
      });
    } catch (err) {
      chrome.test.fail('Failed calling functions on namespace: ' + err);
      return;
    }

    chrome.test.succeed();
  });

  chrome.test.sendMessage('ready');
}
