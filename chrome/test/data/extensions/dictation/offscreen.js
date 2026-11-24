// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let recognition = null;
let streamIdQueue = [];

function delay(ms) {
  return new Promise((resolve) => {
    setTimeout(resolve, ms);
  });
}

async function replayCannedResponse(
    cannedResponse, finalResponse, wordDelay, finalDelay) {
  const streamId = streamIdQueue[0];

  const words = cannedResponse.split(' ');

  for (let i = 1; i <= words.length; ++i) {
    const partialTranscript = words.slice(0, i).join(' ');
    await delay(wordDelay);
    chrome.runtime.sendMessage({
      streamId,
      type: 'partial',
      data: partialTranscript,
    });
  }

  await delay(finalDelay);
  chrome.runtime.sendMessage({
    streamId,
    type: 'final',
    data: finalResponse || cannedResponse,
  });
}

function startRecognition(
    streamId, cannedResponse, finalResponse, wordDelay, finalDelay) {
  streamIdQueue.push(streamId);

  if (cannedResponse) {
    replayCannedResponse(cannedResponse, finalResponse, wordDelay, finalDelay);
    return;
  }

  if (!recognition) {
    recognition = new SpeechRecognition();
    recognition.continuous = true;
    recognition.interimResults = true;
    recognition.maxAlternatives = 1;

    recognition.addEventListener('result', (event) => {
      const results = Array.from(event.results);
      const transcript = results.reduce((acc, r) => {
        return acc + r[0].transcript;
      }, '');
      const isFinal = results.every((r) => {
        return r.isFinal;
      });

      chrome.runtime.sendMessage({
        streamId: streamIdQueue[0],
        type: isFinal ? 'final' : 'partial',
        data: transcript,
      });
    });

    recognition.addEventListener('error', (event) => {
      chrome.runtime.sendMessage({
        streamId: streamIdQueue[0],
        error: `${event.error}: ${event.message}`,
      });
    });
  }

  recognition.start();
}

function stopRecognition(streamId) {
  streamIdQueue = streamIdQueue.filter(id => id !== streamId);

  if (streamIdQueue.length > 0) {
    return;
  }

  if (recognition) {
    recognition.stop();
  }
}

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  const {
    command,
    streamId,
    cannedResponse,
    finalResponse,
    wordDelay,
    finalDelay,
  } = message;
  if (command === 'startStream') {
    startRecognition(
        streamId, cannedResponse, finalResponse, wordDelay, finalDelay);
    sendResponse({status: 'started', streamId});
  } else if (command === 'endStream') {
    stopRecognition(streamId);
  }
});
