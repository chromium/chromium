// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function testSendAudioData() {
  // Sends a series of audio buffers, and verifies we get events for them.
  const expectedEvents = [
    {type: 'start', charIndex: 0}, {'type': 'word', 'charIndex': 10},
    {type: 'word', charIndex: 20}, {type: 'end', charIndex: 39}
  ];

  chrome.ttsEngine.onStop.addListener(() => {});

  let errors = 0;
  chrome.ttsEngine.onSpeakWithAudioStream.addListener(
      (text, options, audioStreamOptions, sendAudioCallback) => {
        // Check the additional parameters.
        chrome.test.assertEq(48000, audioStreamOptions.sampleRate);
        chrome.test.assertEq(128, audioStreamOptions.bufferSize);

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
      });

  function onEvent(event) {
    const expected = expectedEvents.shift();
    chrome.test.assertEq(JSON.stringify(expected), JSON.stringify(event));
    if (event.type == 'end') {
      chrome.test.assertEq(0, expectedEvents.length);
      chrome.test.assertEq(1, errors);
      chrome.test.succeed();
    }
  }

  chrome.tts.speak(
      'this is a test of using audio playback.',
      {voiceName: 'Zach', onEvent: onEvent});
}]);
