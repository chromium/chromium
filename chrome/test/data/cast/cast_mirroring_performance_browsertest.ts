// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Verify |value| is truthy.
 * @param value A value to check for truthiness. Note that this
 *     may be used to test whether |value| is defined or not, and we don't want
 *     to force a cast to boolean.
 */
function assert<T>(value: T, message?: string): asserts value {
  if (value) {
    return;
  }

  throw new Error('Assertion failed' + (message ? `: ${message}` : ''));
}


function startBarcodeAnimation() {
  const RUN_LENGTH_SECONDS: number = 30;
  const audioContext = new AudioContext();
  const audioSource = audioContext.createBufferSource();
  const audioBuffer = audioContext.createBuffer(
      1, audioContext.sampleRate * RUN_LENGTH_SECONDS, audioContext.sampleRate);

  // Renders a barcode representation of the given |frameNumber| to the
  // canvas. The barcode is drawn as follows:
  //
  //   ####    ####    ########    ####         ... ####    ####
  //   ####    ####    ########    ####         ... ####    ####
  //   ####    ####    ########    ####         ... ####    ####
  //   ####    ####    ########    ####         ... ####    ####
  //   0   1   2   3   4   5   6   7   8   9    ... 52  53  54  55
  //   <-----start----><--one-bit-><-zero bit-> ... <----stop---->
  //
  // We use a basic unit, depicted here as four characters wide.  We start
  // with 1u black 1u white 1u black 1u white. (1-4 above) From there on, a
  // "one" bit is encoded as 2u black and 1u white, and a zero bit is
  // encoded as 1u black and 2u white. After all the bits we end the pattern
  // with the same pattern as the start of the pattern.
  //
  // Only the lower 16 bits of frameNumber are drawn.
  const NUM_BARCODE_BITS: number = 16;
  const CANVAS_WIDTH: number = 4 + NUM_BARCODE_BITS * 3 + 4;  // 56.
  const CANVAS_HEIGHT: number = 1;
  let lastFrameNumberRendered: number|null = null;
  function renderBarcodeToCanvas(frameNumber: number) {
    const canvas = document.body.querySelector('canvas');
    assert(canvas);
    const ctx = canvas.getContext('2d');
    assert(ctx);

    if (lastFrameNumberRendered === null) {
      lastFrameNumberRendered = ~frameNumber;
    }
    for (let bitIndex = 0; bitIndex < NUM_BARCODE_BITS; ++bitIndex) {
      const mask = 1 << bitIndex;
      if ((lastFrameNumberRendered & mask) == (frameNumber & mask)) {
        continue;
      }
      // Flip a column of pixels from black to white or white to black
      // to effectively flip the bit in the barcode.
      ctx.fillStyle = (frameNumber & mask) ? '#000000' : '#ffffff';
      ctx.fillRect(bitIndex * 3 + 5, 0, 1, 1);
    }
    lastFrameNumberRendered = frameNumber;
  }

  const FRAMES_PER_SECOND: number = 60;
  const MILLISECONDS_PER_SECOND: number = 1000;
  let firstFrameTime: number = 0;
  let currentFrameNumber: number = -1;
  function drawNextVideoFrame(timestamp: number) {
    if (timestamp >= firstFrameTime) {
      const elapsedSeconds =
          (timestamp - firstFrameTime) / MILLISECONDS_PER_SECOND;
      const frameNumber = Math.trunc(elapsedSeconds * FRAMES_PER_SECOND);
      if (frameNumber != currentFrameNumber) {
        currentFrameNumber = frameNumber;
        renderBarcodeToCanvas(frameNumber);
      }
    }
    requestAnimationFrame(drawNextVideoFrame);
  }

  function startSynchronized(_timestamp: number) {
    const outputTime = audioContext.getOutputTimestamp();
    if (!outputTime.performanceTime) {
      // Audio output has not yet begun pumping data. Try again later.
      requestAnimationFrame(startSynchronized);
      return;
    }
    firstFrameTime = outputTime.performanceTime;
    requestAnimationFrame(drawNextVideoFrame);
  }

  if (audioSource.buffer !== null) {  // Already started?
    return;
  }
  const help = document.getElementById('help');
  assert(help);
  help.style.display = 'none';

  // Set up canvas graphics parameters and render barcode for frame 0.
  const canvas = document.body.querySelector('canvas');
  assert(canvas);
  canvas.width = CANVAS_WIDTH;
  canvas.height = CANVAS_HEIGHT;
  const ctx = canvas.getContext('2d');
  assert(ctx);
  ctx.filter = 'none';
  ctx.imageSmoothingEnabled = false;

  ctx.fillStyle = '#ffffff';
  ctx.fill();
  // Start/Stop sequence bars.
  ctx.fillStyle = '#000000';
  ctx.fillRect(0, 0, 1, 1);
  ctx.fillRect(2, 0, 1, 1);
  ctx.fillRect(CANVAS_WIDTH - 4, 0, 1, 1);
  ctx.fillRect(CANVAS_WIDTH - 2, 0, 1, 1);
  // Bars representing all bits set to zero.
  for (let x = 4; x < 52; x += 3) {
    ctx.fillRect(x, 0, 1, 1);
  }

  // Populate audio barcodes, as a 16-bit number. Based on EncodeTimestamp
  // function from media/cast/test/utility/audio_utility.cc.
  const BASE_FREQUENCY = 200.0;
  const TWO_PI_OVER_BASE_FREQUENCY = 2.0 * Math.PI / audioBuffer.sampleRate;
  const channelData = audioBuffer.getChannelData(0);
  let i = 0;
  for (let frameNumber = 0; i < channelData.length; ++frameNumber) {
    // Gray-code the frameNumber.
    const code = (frameNumber >> 1) ^ frameNumber;

    // Determine which sine waves to render.
    const SENSE_FREQUENCY = BASE_FREQUENCY * (NUM_BARCODE_BITS + 1);
    let freqs = [SENSE_FREQUENCY];
    for (let j = 0; j < NUM_BARCODE_BITS; ++j) {
      if ((code >> j) & 1) {
        freqs.push(BASE_FREQUENCY * (j + 1));
      }
    }

    // Determine the index after the last sample to be rendered.
    const end = Math.min(
        Math.round(
            ((frameNumber + 1) / FRAMES_PER_SECOND) * audioBuffer.sampleRate),
        channelData.length);

    // Render the samples by mixing the selected sine waves.
    for (; i < end; ++i) {
      let sample = 0.0;
      for (let j = 0; j < freqs.length; ++j) {
        sample += Math.sin(TWO_PI_OVER_BASE_FREQUENCY * i * freqs[j]!);
      }
      sample /= NUM_BARCODE_BITS + 1;  // Normalize to [-1.0,1.0].
      channelData[i] = sample;
    }
  }

  audioSource.buffer = audioBuffer;
  audioSource.connect(audioContext.destination);
  audioSource.start();

  requestAnimationFrame(startSynchronized);
}

const mainBody = document.getElementById('mainBody');
assert(mainBody);
mainBody.addEventListener('click', function() {
  startBarcodeAnimation();
});
