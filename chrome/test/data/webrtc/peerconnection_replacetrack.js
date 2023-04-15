/**
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * The resolver has a |promise| that can be resolved or rejected using |resolve|
 * or |reject|.
 */
// TODO(hbos): Remove when no longer needed. https://crbug.com/793808
class Resolver {
  constructor() {
    let promiseResolve;
    let promiseReject;
    this.promise = new Promise(function(resolve, reject) {
      promiseResolve = resolve;
      promiseReject = reject;
    });
    this.resolve = promiseResolve;
    this.reject = promiseReject;
  }
}

// TODO(hbos): Make this an external/wpt/webrtc/ test when video elements are
// updated by received webrtc streams in content_shell. https://crbug.com/793808
async function testRTCRtpSenderReplaceTrackSendsNewVideoTrack() {
  const redCanvas = document.getElementById('redCanvas');
  const redCanvasStream = redCanvas.captureStream(10);
  const blueCanvas = document.getElementById('blueCanvas');
  const blueCanvasStream = blueCanvas.captureStream(10);
  const remoteVideo = document.getElementById('remote-view');
  const remoteVideoCanvas = document.getElementById('whiteCanvas');

  const caller = new RTCPeerConnection();
  const callee = new RTCPeerConnection();

  // Connect and send "redCanvas" to callee.
  const sender = caller.addTrack(redCanvasStream.getTracks()[0],
                                 redCanvasStream);
  const connectPromise = connect(caller, callee);
  const trackEvent = await eventAsAsyncFunction(callee, 'ontrack');
  remoteVideo.srcObject = trackEvent.streams[0];
  await connectPromise;

  // Ensure a red frame is sent by redrawing the canvas while polling.
  function fillRedCanvas() { fillCanvas(redCanvas, 'red'); }
  let receivedColor = await pollNextVideoColor(
      fillRedCanvas, remoteVideo, remoteVideoCanvas);
  if (receivedColor != 'red')
    throw new Error('Expected red, but received: ' + receivedColor);

  // Send "blueCanvas" to callee using the existing sender.
  await sender.replaceTrack(blueCanvasStream.getTracks()[0]);

  // Ensure a blue frame is sent by redrawing the canvas while polling.
  function fillBlueCanvas() { fillCanvas(blueCanvas, 'blue'); }
  receivedColor = await pollNextVideoColor(
      fillBlueCanvas, remoteVideo, remoteVideoCanvas);
  if (receivedColor != 'blue')
    throw new Error('Expected blue, but received: ' + receivedColor);

  return logAndReturn('test-passed');
}

// Internals.

/** @private */
async function connect(caller, callee) {
  caller.onicecandidate = (e) => {
    if (e.candidate)
      callee.addIceCandidate(new RTCIceCandidate(e.candidate));
  }
  callee.onicecandidate = (e) => {
    if (e.candidate)
      caller.addIceCandidate(new RTCIceCandidate(e.candidate));
  }
  let offer = await caller.createOffer();
  await caller.setLocalDescription(offer);
  await callee.setRemoteDescription(offer);
  let answer = await callee.createAnswer();
  await callee.setLocalDescription(answer);
  return caller.setRemoteDescription(answer);
}

/**
 * Makes the next |object[eventname]| event resolve the returned promise with
 * the event argument and resets the event handler to null.
 * @private
 */
async function eventAsAsyncFunction(object, eventname) {
  const resolver = new Resolver();
  object[eventname] = e => {
    object[eventname] = null;
    resolver.resolve(e);
  }
  return resolver.promise;
}

/**
 * Updates the canvas, filling it with |color|, e.g. 'red', 'lime' or 'blue'.
 * @private
 */
function fillCanvas(canvas, color) {
  const canvasContext = canvas.getContext('2d');
  canvasContext.fillStyle = color;
  canvasContext.fillRect(0, 0, canvas.width, canvas.height);
}

/**
 * Gets the dominant color of the center of the canvas, meaning the color that
 * is closest to that pixel's color amongst: 'black', 'white', 'red', 'lime' and
 * 'blue'.
 * @private
 */
function getDominantCanvasColor(canvas) {
  const colorData = canvas.getContext('2d').getImageData(
      Math.floor(canvas.width / 2), Math.floor(canvas.height / 2), 1, 1).data;

  const dominantColors = [
    { name: 'black', colorData: [0, 0, 0] },
    { name: 'white', colorData: [255, 255, 255] },
    { name: 'red', colorData: [255, 0, 0] },
    { name: 'lime', colorData: [0, 255, 0] },
    { name: 'blue', colorData: [0, 0, 255] },
  ];
  function getColorDistanceSquared(colorData1, colorData2) {
    const colorDiff = [ colorData2[0] - colorData1[0],
                        colorData2[1] - colorData1[1],
                        colorData2[2] - colorData1[2] ];
    return colorDiff[0] * colorDiff[0] +
           colorDiff[1] * colorDiff[1] +
           colorDiff[2] * colorDiff[2];
  }
  let dominantColor = dominantColors[0];
  let dominantColorDistanceSquared =
      getColorDistanceSquared(dominantColor.colorData, colorData);
  for (let i = 1; i < dominantColors.length; ++i) {
    const colorDistanceSquared =
        getColorDistanceSquared(dominantColors[i].colorData, colorData);
    if (colorDistanceSquared < dominantColorDistanceSquared) {
      dominantColor = dominantColors[i];
      dominantColorDistanceSquared = colorDistanceSquared;
    }
  }
  return dominantColor.name;
}

/**
 * Polls the video's dominant color (see getDominantCanvasColor()) until a color
 * different than the initial color is retrieved, resolving the returned promise
 * with the new color name. The video color is read by drawing the video onto a
 * canvas and reading the color of the canvas. Before each time the color is
 * polled, doWhilePolling() is invoked.
 * @private
 */
async function pollNextVideoColor(doWhilePolling, video, canvas) {
  canvas.getContext('2d').drawImage(video, 0, 0);
  const initialColor = getDominantCanvasColor(canvas);
  const resolver = new Resolver();
  function checkColor() {
    doWhilePolling();
    canvas.getContext('2d').drawImage(video, 0, 0);
    const color = getDominantCanvasColor(canvas);
    if (color != initialColor) {
      resolver.resolve(color);
      return;
    }
    setTimeout(checkColor, 0);
  }
  setTimeout(checkColor, 0);
  return resolver.promise;
}
