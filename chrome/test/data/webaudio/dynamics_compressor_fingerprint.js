// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Computes the DynamicsCompressor fingerprint and sends it back to the test via
// `sendValueToTest`.

async function computeFingerprint() {
  const context = new OfflineAudioContext(
      {numberOfChannels: 1, length: 44100, sampleRate: 44100});
  if (!context) return 0.0;
  const oscillator = new OscillatorNode(context);
  const compressor = new DynamicsCompressorNode(context);
  // For this test, it's ok to mix the stereo compressor output to mono for the
  // final result.
  oscillator.connect(compressor).connect(context.destination);
  oscillator.start(0);
  const renderedBuffer = await context.startRendering();
  const chanData = renderedBuffer.getChannelData(0);
  let result = 0;
  for (const elem of chanData) {
    result += elem;
  }
  return result;
};

window.addEventListener('load', async () => {
  const fingerprint = await computeFingerprint();
  sendValueToTest(fingerprint);
});
