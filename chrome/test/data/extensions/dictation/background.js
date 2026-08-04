// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const OFFSCREEN_PATH = 'offscreen.html';

const startedStreams = new Map();
const streamStartWaiters = new Map();
const endedStreams = new Map();
const streamEndWaiters = new Map();
const updatedContexts = new Map();
const contextUpdateWaiters = new Map();

function notifyStreamStarted(streamId, details) {
  startedStreams.set(streamId, details);
  const waiters = streamStartWaiters.get(streamId);
  if (waiters) {
    for (const resolve of waiters) {
      resolve(details);
    }
    streamStartWaiters.delete(streamId);
  }
}

globalThis.waitForStreamStart = function(streamId) {
  if (startedStreams.has(streamId)) {
    return Promise.resolve(startedStreams.get(streamId));
  }
  return new Promise((resolve) => {
    if (!streamStartWaiters.has(streamId)) {
      streamStartWaiters.set(streamId, []);
    }
    streamStartWaiters.get(streamId).push(resolve);
  });
};

function notifyStreamEnded(streamId, details) {
  endedStreams.set(streamId, details);
  const waiters = streamEndWaiters.get(streamId);
  if (waiters) {
    for (const resolve of waiters) {
      resolve(details);
    }
    streamEndWaiters.delete(streamId);
  }
}

globalThis.waitForStreamEnd = function(streamId) {
  if (endedStreams.has(streamId)) {
    return Promise.resolve(endedStreams.get(streamId));
  }
  return new Promise((resolve) => {
    if (!streamEndWaiters.has(streamId)) {
      streamEndWaiters.set(streamId, []);
    }
    streamEndWaiters.get(streamId).push(resolve);
  });
};

function notifyContextUpdated(streamId, details) {
  updatedContexts.set(streamId, details);
  const waiters = contextUpdateWaiters.get(streamId);
  if (waiters) {
    for (const resolve of waiters) {
      resolve(details);
    }
    contextUpdateWaiters.delete(streamId);
  }
}

globalThis.waitForContextUpdate = function(streamId) {
  if (updatedContexts.has(streamId)) {
    return Promise.resolve(updatedContexts.get(streamId));
  }
  return new Promise((resolve) => {
    if (!contextUpdateWaiters.has(streamId)) {
      contextUpdateWaiters.set(streamId, []);
    }
    contextUpdateWaiters.get(streamId).push(resolve);
  });
};

async function isManualTest() {
  const options = await chrome.storage.local.get({manualTest: false});
  return options.manualTest;
}

async function hasOffscreenDocument() {
  const offscreenUrl = chrome.runtime.getURL(OFFSCREEN_PATH);
  const existingContexts = await chrome.runtime.getContexts({
    contextTypes: ['OFFSCREEN_DOCUMENT'],
    documentUrls: [offscreenUrl],
  });

  return existingContexts.length > 0;
}

let creatingDocument = null;
async function setupOffscreenDocument() {
  if (await hasOffscreenDocument()) {
    return;
  }

  if (creatingDocument) {
    await creatingDocument;
  } else {
    creatingDocument = chrome.offscreen.createDocument({
      url: OFFSCREEN_PATH,
      reasons: ['USER_MEDIA'],
      justification: 'Dictation test extension requires microphone access',
    });
    await creatingDocument;
    creatingDocument = null;
  }
}

async function startStream(streamId) {
  chrome.dictationPrivate.setStreamState(
      {streamId, state: chrome.dictationPrivate.StreamState.INITIALIZING});

  const optionsItems = await chrome.storage.local.get(
      {cannedResponse: '', wordDelay: 0, finalDelay: 0, failOnStart: false});

  if (optionsItems.failOnStart) {
    chrome.dictationPrivate.setStreamState({
      streamId,
      state: chrome.dictationPrivate.StreamState.FAILED,
    });
    return;
  }

  await setupOffscreenDocument();
  const startResult = await chrome.runtime.sendMessage({
    command: 'startStream',
    streamId,
    cannedResponse: optionsItems.cannedResponse,
    wordDelay: optionsItems.wordDelay,
    finalDelay: optionsItems.finalDelay,
  });
  chrome.dictationPrivate.setStreamState({
    streamId,
    state: (startResult.status === 'started') ?
        chrome.dictationPrivate.StreamState.TRANSCRIBING :
        chrome.dictationPrivate.StreamState.FAILED,
  });
}

async function endStream(streamId) {
  if (!await hasOffscreenDocument()) {
    return;
  }

  chrome.runtime.sendMessage({command: 'endStream', streamId});
}

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  const {streamId, type, data, error} = message;

  console.info(
      `[Dictation Result] type: "${type}", data: "${data}", error: "${error}"`);

  if (error) {
    chrome.dictationPrivate.setStreamState(
        {streamId, state: chrome.dictationPrivate.StreamState.FAILED});
  } else if (
      type === chrome.dictationPrivate.TranscriptionType.FINAL ||
      type === chrome.dictationPrivate.TranscriptionType.PARTIAL) {
    chrome.dictationPrivate.updateTranscription({streamId, type, data});

    // Simulate speech audio level changes as the transcription updates.
    chrome.dictationPrivate.updateAudioLevel(1.0);
    setTimeout(() => {
      chrome.dictationPrivate.updateAudioLevel(0.0);
    }, 250);
  }
});

chrome.dictationPrivate.onStartStream.addListener(async (details) => {
  // In a manual test, the test code itself simulates the extension API calls
  // so avoid calling into any of the extension code.
  if (await isManualTest()) {
    notifyStreamStarted(details.streamId, details);
    return;
  }

  console.info(
      '[onStartStream] Selected text received from Chrome:',
      details.context.editableContent);
  startStream(details.streamId);
});

chrome.dictationPrivate.onEndStream.addListener(async (details) => {
  // In a manual test, the test code itself simulates the extension API calls
  // so avoid calling into any of the extension code.
  if (await isManualTest()) {
    notifyStreamEnded(details.streamId, details);
    return;
  }

  const {streamId} = details;

  await endStream(streamId);
  chrome.dictationPrivate.setStreamState(
      {streamId, state: chrome.dictationPrivate.StreamState.COMPLETE});
});

chrome.dictationPrivate.onContextUpdate.addListener(async (details) => {
  if (await isManualTest()) {
    if (!startedStreams.has(details.streamId)) {
      chrome.test.fail(
          'Context update received before start stream for stream ' +
          details.streamId);
      return;
    }
    notifyContextUpdated(details.streamId, details);
    return;
  }
  console.info('[onContextUpdate] Context updated:', details.context);
});
