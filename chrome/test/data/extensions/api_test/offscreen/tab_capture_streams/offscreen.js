// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let stream;
// Starts capture with the given `streamId`.
async function startCapture(streamId) {
  stream =
      await navigator.mediaDevices.getUserMedia(
          {
            audio: false,
            video: {
              mandatory: {
                chromeMediaSource: 'tab',
                chromeMediaSourceId: streamId
              }
            }
          });

  if (!stream || stream.getVideoTracks().length == 0) {
    throw new Error('Failed to get stream');
  }
}

// Stops the currently-active capture.
function stopCapture() {
  if (!stream) {
    throw new Error('Capture never started');
  }
  stream.getVideoTracks()[0].stop();
}

// Handles incoming messages, replying with 'success' or an appropriate
// error.
async function handleMessage(msg, reply) {
  let response = 'success';
  try {
    if (msg.command == 'capture') {
      if (!msg.streamId) {
        throw new Error('No stream ID received');
      }
      await startCapture(msg.streamId);
    } else if (msg.command == 'stop') {
      stopCapture();
    } else {
      throw new Error(
          `Unexpected message: ${JSON.stringify(message)}`);
    }
  } catch (e) {
    response = e.toString();
  }
  reply(response);
}

// Somewhat clunky: `sendMessage()` requires us to return true from the
// message handler in order to reply asynchronously. We have to wrap
// our async function handler in a sync function to satisfy this
// (otherwise, the listener returns a promise, which closes the
// channel).
chrome.runtime.onMessage.addListener((msg, sender, reply) => {
  setTimeout(() => { handleMessage(msg, reply); }, 0);
  return true;
});
