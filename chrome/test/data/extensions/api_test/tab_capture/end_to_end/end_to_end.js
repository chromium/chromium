// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests here cover the end-to-end functionality of tab capturing and
// playback as video.  The page generates video test patterns that rotate
// cyclicly, and the rendering output of the tab is captured into a
// local MediaStream.  This stream is then piped into a video element for
// playback, and a canvas is used to examine the frames of the video for
// expected content.  The stream may be plumbed one of two ways, depending on
// the 'method' query param:
//
//   local:  LocalMediaStream --> DOM Video Element
//   webrtc: LocalMediaStream --> PeerConnection (sender)
//             --> PeerConnection (receiver) --> DOM Video Element
//
// There are two rounds of testing: In each round, video frames are received and
// monitored, and the round successfully completes once each of the expected
// patterns is observed. Between the two rounds, the MediaStream is disconnected
// from its consumer, which places the tab capture "device" into a suspend mode.
var testRoundNumber = 0;

// The test pattern cycles as a color fill of red, then green, then blue.
var colors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];
var curIdx = 0;

// Capture parameters.
var width = 64;
var height = 48;
var frameRate = 15;

// The stream to playback in the video element.
var receiveStream = null;

// waitForExpectedColors() removes elements from this array as each is observed.
// When it becomes empty, the test succeeds.
var expectedColors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];

function updateTestPattern() {
  if (!this.canvas) {
    this.canvas = document.createElement("canvas");
    this.canvas.width = 320;
    this.canvas.height = 200;
    this.canvas.style.position = "absolute";
    this.canvas.style.top = "0px";
    this.canvas.style.left = "0px";
    this.canvas.style.width = "100%";
    this.canvas.style.height = "100%";
    document.body.appendChild(this.canvas);
  }
  var context = this.canvas.getContext("2d");
  // Fill with solid color.
  context.fillStyle = "rgb(" + colors[curIdx] + ")";
  context.fillRect(0, 0, this.canvas.width, this.canvas.height);
  // Draw the circle that moves around the page.
  context.fillStyle = "rgb(" + colors[(curIdx + 1) % colors.length] + ")";
  context.beginPath();
  if (!this.frameNumber) {
    this.frameNumber = 1;
  } else {
    ++this.frameNumber;
  }
  var i = this.frameNumber % 200;
  var t = (this.frameNumber + 3000) * (0.01 + i / 8000.0);
  var x = (Math.sin(t) * 0.45 + 0.5) * this.canvas.width;
  var y = (Math.cos(t * 0.9) * 0.45 + 0.5) * this.canvas.height;
  context.arc(x, y, 16, 0, 2 * Math.PI, false);
  context.closePath();
  context.fill();
}

function renderTestPatternLoop() {
  requestAnimationFrame(renderTestPatternLoop);
  updateTestPattern();

  if (!this.stepTimeMillis) {
    this.stepTimeMillis = 100;
  }
  var now = new Date().getTime();
  if (!this.nextSteppingAt) {
    this.nextSteppingAt = now + this.stepTimeMillis;
  } else if (now >= this.nextSteppingAt) {
    ++curIdx;
    if (curIdx >= colors.length) {  // Completed a cycle.
      curIdx = 0;
      // Increase the wait time between switching test patterns for overloaded
      // bots that aren't capturing all the frames of video. Only do this for
      // the first test phase, since increases shouldn't be needed after that.
      if (testRoundNumber == 0) {
        this.stepTimeMillis *= 1.25;
      }
    }
    this.nextSteppingAt = now + this.stepTimeMillis;
  }
}

function waitForExpectedColors(colorDeviation) {
  // At the start of a round of testing, create the video and canvas elements,
  // but no need to append them to the DOM.
  if (!this.video) {
    this.video = document.createElement("video");
    this.video.width = width;
    this.video.height = height;
    this.video.addEventListener("error", chrome.test.fail);
    this.video.srcObject = receiveStream;
    this.video.play();

    this.readbackCanvas = document.createElement("canvas");
    this.readbackCanvas.width = width;
    this.readbackCanvas.height = height;
  }

  // Only bother examining a video frame if the video timestamp has advanced.
  var currentVideoTimestamp = this.video.currentTime;
  if (!this.lastVideoTimestamp ||
      this.lastVideoTimestamp < currentVideoTimestamp) {
    this.lastVideoTimestamp = currentVideoTimestamp;

    // Grab a snapshot of the center pixel of the video.
    var ctx = this.readbackCanvas.getContext("2d");
    ctx.drawImage(video, 0, 0, width, height);
    var imageData = ctx.getImageData(width / 2, height / 2, 1, 1);
    var pixel = [ imageData.data[0], imageData.data[1], imageData.data[2] ];

    // Does the pixel match one of the expected colors?
    for (var i = 0; i < expectedColors.length; ++i) {
      var curColor = expectedColors[i];
      if (Math.abs(pixel[0] - curColor[0]) <= colorDeviation &&
          Math.abs(pixel[1] - curColor[1]) <= colorDeviation &&
          Math.abs(pixel[2] - curColor[2]) <= colorDeviation) {
        console.debug(`${testRoundNumber == 0 ? 'First' : 'Second'} round: ` +
            `Observed expected color RGB(${curColor}) in the video as ` +
            `RGB(${pixel})`);
        expectedColors.splice(i, 1);
      }
    }
  }

  if (expectedColors.length == 0) {
    // Successful end of the current test round. If the first round, sleep, then
    // execute the second round. If the second round, then the whole test has
    // succeeded.
    if (testRoundNumber == 0) {
      // Destroy the video, which will disconnect the consumer of the
      // MediaStream.
      this.video.removeEventListener("error", chrome.test.fail);
      this.video.srcObject = null;
      this.video = null;

      // Wait one second, then execute the second round of testing. This tests
      // the suspend/resume functionality of tab capture (w.r.t. a MediaStream
      // having no consumers, and then being re-used with a new consumer).
      console.debug('First round succeeded! Now testing suspend/resume...');
      setTimeout(function() {
        ++testRoundNumber;
        for (const color of colors) {
          expectedColors.push(color);
        }
        waitForExpectedColors(colorDeviation);
      }, 1000);
    } else {
      chrome.test.succeed();
    }
  } else {
    setTimeout(function () { waitForExpectedColors(colorDeviation); },
               1000 / frameRate);
  }
}

chrome.test.runTests([
  function endToEndTest() {
    // The receive port changes between browser_test invocations, and is passed
    // as an query parameter in the URL.
    var transportMethod;  // Should be: local or webrtc.
    var colorDeviation;  // How far from the expected intensity ([0,255] scale)?
    try {
      transportMethod = window.location.search.match(/(\?|&)method=(\w+)/)[2];
      chrome.test.assertTrue(transportMethod == 'local' ||
                             transportMethod == 'webrtc');
      colorDeviation = parseInt(
          window.location.search.match(/(\?|&)colorDeviation=(\d+)/)[2]);
      chrome.test.assertTrue(colorDeviation >= 0 && colorDeviation <= 255);
    } catch (err) {
      chrome.test.fail("Error parsing query params -- " + err.message);
      return;
    }

    // Start rendering test patterns.
    renderTestPatternLoop();

    chrome.tabCapture.capture(
        { video: true,
          audio: true,
          videoConstraints: {
            mandatory: {
              minWidth: width,
              minHeight: height,
              maxWidth: width,
              maxHeight: height,
              maxFrameRate: frameRate,
            }
          }
        },
        function remoteTheStream(captureStream) {
          chrome.test.assertTrue(!!captureStream);
          if (transportMethod == 'local') {
            receiveStream = captureStream;
            waitForExpectedColors(colorDeviation);
          } else if (transportMethod == 'webrtc') {
            var sender = new RTCPeerConnection();
            var receiver = new RTCPeerConnection();
            sender.onicecandidate = function (event) {
              if (event.candidate) {
                receiver.addIceCandidate(new RTCIceCandidate(event.candidate));
              }
            };
            receiver.onicecandidate = function (event) {
              if (event.candidate) {
                sender.addIceCandidate(new RTCIceCandidate(event.candidate));
              }
            };
            receiver.onaddstream = function (event) {
              receiveStream = event.stream;
              waitForExpectedColors(colorDeviation);
            };
            sender.addStream(captureStream);
            sender.createOffer(function (sender_description) {
              sender.setLocalDescription(sender_description);
              receiver.setRemoteDescription(sender_description);
              receiver.createAnswer(function (receiver_description) {
                receiver.setLocalDescription(receiver_description);
                sender.setRemoteDescription(receiver_description);
              }, function (error) {
              });
            }, function (error) {
            });
          } else {
            chrome.test.fail("Unknown transport method: " + transportMethod);
          }
        });
  }
]);
