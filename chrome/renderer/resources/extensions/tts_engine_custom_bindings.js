// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the ttsEngine API.

bindingUtil.registerEventArgumentMassager('ttsEngine.onSpeak',
                                          function(args, dispatch) {
  var text = args[0];
  var options = args[1];
  var requestId = args[2];
  if (args.length !== 3) {
    throw new Error('Unexpected number of args: ' + args.length);
  }

  var sendTtsEvent = function(event) {
    chrome.ttsEngine.sendTtsEvent(requestId, event);
  };
  dispatch([text, options, sendTtsEvent]);
});

let currentRequestId;
bindingUtil.registerEventArgumentMassager('ttsEngine.onSpeakWithAudioStream',
                                          function(args, dispatch) {
  let text = args[0];
  let options = args[1];
  let requestId = args[2];
  let audioStreamOptions = args[3];
  if (args.length !== 4) {
    throw new Error('Unexpected number of args: ' + args.length);
  }

  currentRequestId = requestId;

  const sendTtsAudio = function(audioBufferParams) {
    const {audioBuffer, charIndex, isLastBuffer} = audioBufferParams;
    if (currentRequestId == requestId) {
      if (!audioBuffer) {
        throw new Error('Invalid audio buffer: ' + audioBuffer);
      }

      if (audioBuffer.length !== audioStreamOptions.bufferSize) {
        throw new Error(
            `Invalid audio buffer size ` +
            `${audioBuffer.length}; expected == ${
                audioStreamOptions.length}`);
      }

      chrome.ttsEngine.sendTtsAudio(requestId, {
        audioBuffer,
        charIndex: charIndex !== undefined ? charIndex : -1,
        isLastBuffer: !!isLastBuffer
      });
    }
  };

  const sendError = function(errorMessage = '') {
    chrome.ttsEngine.sendTtsEvent(requestId, {type: 'error', errorMessage});
  };

  dispatch([text, options, audioStreamOptions, sendTtsAudio, sendError]);
});
