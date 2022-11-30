// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testSendAudioData() {
    // Sends a series of audio buffers, and verifies we get events for them.
    const expectedEvents = [
      {type: 'start', charIndex: 0}, {'type': 'word', 'charIndex': 10},
      {type: 'word', charIndex: 20}, {type: 'end', charIndex: 39}
    ];

    chrome.ttsEngine.onStop.addListener(() => {});

    let errors = 0;
    const listener = (text, options, audioStreamOptions, sendAudioCallback) => {
      // Check the additional parameters.
      chrome.test.assertTrue(audioStreamOptions.sampleRate >= 1000);
      chrome.test.assertTrue(audioStreamOptions.bufferSize >= 100);

      const buffer = new Float32Array(audioStreamOptions.bufferSize);

      // Start event.
      sendAudioCallback({audioBuffer: buffer});

      // A word event.
      sendAudioCallback({audioBuffer: buffer, charIndex: 10});

      // A word event.
      sendAudioCallback({audioBuffer: buffer, charIndex: 20});

      // Invalid audio buffer length.
      try {
        sendAudioCallback({audioBuffer: [0, 0]});
      } catch (e) {
        errors++;
      }

      // End event.
      sendAudioCallback(
          {audioBuffer: buffer, charIndex: 30, isLastBuffer: true});
    };
    chrome.ttsEngine.onSpeakWithAudioStream.addListener(listener);

    function onEvent(event) {
      const expected = expectedEvents.shift();
      chrome.test.assertEq(JSON.stringify(expected), JSON.stringify(event));
      if (event.type == 'end') {
        chrome.test.assertEq(0, expectedEvents.length);
        chrome.test.assertEq(1, errors);
        chrome.ttsEngine.onSpeakWithAudioStream.removeListener(listener);
        chrome.test.succeed();
      }
    }

    chrome.tts.speak(
        'this is a test of using audio playback.',
        {voiceName: 'Zach', onEvent: onEvent});
  },

  async function testErrorEvents() {
    // Errors, when sent, invalidate the utterance (id) so all other callbacks
    // sent by the engine are not sent.
    let toSend;
    let bufferSize;
    const listener =
        (text, options, audioStreamOptions, sendAudioCallback,
         sendErrorCallback) => {
          bufferSize = audioStreamOptions.bufferSize;
          if (toSend.errorMessage) {
            sendErrorCallback(toSend.errorMessage);
          } else {
            sendAudioCallback(toSend);
          }
        };
    chrome.ttsEngine.onSpeakWithAudioStream.addListener(listener);

    // Note that 'start' gets sent by the controller regardless of what is sent
    // here, so 'start' can be sent in any order relative to 'error'.

    // Sent an error.
    toSend = {errorMessage: 'Error encountered'};
    await new Promise(r => {chrome.tts.speak('asdf', {
                        voiceName: 'Zach',
                        onEvent: (event) => {
                          if (event.errorMessage === 'Error encountered') {
                            chrome.test.assertEq('error', event.type);
                            r();
                          }
                        }
                      })});

    // Sanity check another utterance with no errors gets cycled back correctly.
    // The check asserts the event type directly without waitiing to catch if
    // 'error' gets sent and causes this to flake.
    toSend = {type: 'start', audioBuffer: new Float32Array(bufferSize)};
    await new Promise(r => {chrome.tts.speak('asdf', {
                        voiceName: 'Zach',
                        onEvent: (event) => {
                          chrome.test.assertEq('start', event.type);
                          r();
                        }
                      })});

    chrome.test.succeed();
  }
]);
