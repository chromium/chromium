// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Registering chrome.runtime message listener...');

chrome.runtime.onMessageExternal.addListener((request, from, sendResponse) => {
  if (!request.start) {
    return;
  }

  const kCallbackTimeoutMillis = 10000;

  if (window.currentStream) {
    sendResponse({success: false, reason: 'Already started!'});
    return;
  }

  // Start capture and wait up to kCallbackTimeoutMillis for it to start.
  const startCapturePromise = new Promise((resolve, reject) => {
    const timeoutId = setTimeout(() => {
      reject(Error('chrome.tabCapture.capture() did not call back'));
    }, kCallbackTimeoutMillis);

    const captureOptions = {
      video: true,
      audio: false,
      videoConstraints: {
        mandatory: {
          minWidth: 1920,
          minHeight: 1080,
          maxWidth: 1920,
          maxHeight: 1080,
          maxFrameRate: 60,
        }
      }
    };
    console.log('Starting tab capture...');
    chrome.tabCapture.capture(captureOptions, captureStream => {
      clearTimeout(timeoutId);
      if (captureStream) {
        console.log('Started tab capture.');
        resolve(captureStream);
      } else {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError);
        } else {
          reject(Error('null stream'));
        }
      }
    });
  });

  // Then, start streaming the data via WebRTC (or nothing, depending on the
  // request).
  const startStreamingPromise = startCapturePromise.then(captureStream => {
    return new Promise((resolve, reject) => {
      if (!request.passThroughWebRTC) {
        resolve(captureStream);
        return;
      }

      const timeoutId = setTimeout(() => {
        reject(Error('receiver did not get a stream'));
      }, kCallbackTimeoutMillis);

      console.log('Routing through RTCPeerConnection...');
      const sender = new RTCPeerConnection();
      const receiver = new RTCPeerConnection();
      sender.addStream(captureStream);
      sender.onicecandidate = (event) => {
        if (event.candidate) {
          receiver.addIceCandidate(new RTCIceCandidate(event.candidate));
        }
      };
      receiver.onicecandidate = (event) => {
        if (event.candidate) {
          sender.addIceCandidate(new RTCIceCandidate(event.candidate));
        }
      };
      receiver.onaddstream = (event) => {
        console.log('Receiving stream...');
        resolve(event.stream);
      };
      sender.createOffer((sender_description) => {
        sender.setLocalDescription(sender_description);
        receiver.setRemoteDescription(sender_description);
        receiver.createAnswer((receiver_description) => {
          receiver.setLocalDescription(receiver_description);
          sender.setRemoteDescription(receiver_description);
        }, reject);
      }, reject);

      window.rtcSender = sender;
      window.rtcReceiver = receiver;
    });
  });

  // Plug the capture into a video element to finish assembling the end-to-end
  // system.
  startStreamingPromise.then(receiveStream => {
    console.log('Starting receiver video playback...');
    window.currentStream = receiveStream;
    window.receiverVideo = document.createElement('video');
    window.receiverVideo.srcObject = receiveStream;
    window.receiverVideo.play();

    console.log('Sending success response...');
    sendResponse({success: true});
  }).catch(error => {
    console.log('Sending error response...');
    let errorMessage;
    if (typeof error === 'object' &&
        ('stack' in error || 'message' in error)) {
      errorMessage = (error.stack || error.message);
    } else {
      errorMessage = String(error);
    }
    sendResponse({success: false, reason: errorMessage});
  });

  return true;  // Indicate that sendResponse() will be called asynchronously.
});
