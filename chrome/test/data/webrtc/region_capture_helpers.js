/**
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

"use strict";

///////////////////////////
// Test page management. //
///////////////////////////

let role;
function setRole(value) {
  role = value;
}

async function startEmbeddingFrame(url) {
  const waiter = waitForMessage('embedding-done');
  document.getElementById("embedded_frame").src = url;
  const message = await waiter;
  return message.messageType;
}

function registerEmbeddedListeners() {
  window.addEventListener("message", (event) => {
    const type = event.data.messageType;
    const reply = (message) => {
      event.source.postMessage(message, "*");
    };
    if (type == "setup-mailman") {
      setUpMailman(event.data.url).then(reply);
    } else if (type == "start-capture") {
      startCapture().then((result) => {
        reply({messageType: "start-capture-complete", result});
      });
    } else if (type == "produce-crop-target") {
      cropTargetFromElement("embedded", event.data.element)
        .then((result) => {
          reply({messageType: "produce-crop-target-complete", result});
        });
    } else if (type == "crop-to") {
      cropTo(event.data.cropTarget, event.data.targetFrame,
             event.data.targetTrackStr)
        .then((result) => {
          reply({messageType: "crop-to-complete", result});
        });
    } else if (type == 'create-new-element') {
      createNewElement(event.data.targetFrame, event.data.tag, event.data.id)
        .then((result) => {
          reply({messageType: "create-new-element-complete", result});
        });
    } else if (type == 'start-second-capture') {
      startSecondCapture(event.data.targetFrame)
        .then((result) => {
          reply({messageType: "start-second-capture-complete", result});
        });
    } else if (type == 'stop-capture') {
      stopCapture(event.data.targetFrame, event.data.targetTrack)
        .then((result) => {
          reply({messageType: "stop-complete", result});
        });
    }
  });
}

// Allows creating new elements for which new crop-targets may be created.
async function createNewElement(targetFrame, tag, id) {
  if (role == targetFrame) {
    const newElement = document.createElement(tag);
    newElement.id = id;
    document.body.appendChild(newElement);
    return `${role}-new-element-success`;
  } else {
    if (role != "top-level") {
      return `${role}-new-element-error`;
    }
    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("create-new-element-complete");
    embedded_frame.contentWindow.postMessage(
        {
          messageType: 'create-new-element',
          targetFrame: targetFrame,
          tag: tag,
          id: id
        },
        '*');
    const message = await waiter;
    return message.result;
  }
}

function animate(element) {
  const animationCallback = function() {
    element.innerHTML = parseInt(element.innerHTML) + 1;
  };
  setInterval(animationCallback, 20);
}

/////////////////////////////////////////
// Main actions from C++ test fixture. //
/////////////////////////////////////////

async function startCapture() {
  if (track || trackClone) {
    return "error-multiple-captures";
  }

  try {
    const stream = await navigator.mediaDevices.getDisplayMedia({
      video: true,
      selfBrowserSurface: "include"
    });
    [track] = stream.getVideoTracks();
    return `${role}-capture-success`;
  } catch (e) {
    return `${role}-capture-failure`;
  }
}

async function startSecondCapture(targetFrame) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      return "error";
    }

    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("start-second-capture-complete");
    embedded_frame.contentWindow.postMessage({
        messageType: "start-second-capture",
        targetFrame: targetFrame
      }, "*");
    const message = await waiter;
    return message.result;
  }

  try {
    const stream = await navigator.mediaDevices.getDisplayMedia({
      video: true,
      selfBrowserSurface: "include"
    });
    [otherCaptureTrack] = stream.getVideoTracks();
    return `${role}-second-capture-success`;
  } catch (e) {
    return `${role}-second-capture-failure`;
  }
}

async function stopCapture(targetFrame, targetTrack) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      return "error";
    }

    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("stop-capture-complete");
    embedded_frame.contentWindow.postMessage({
        messageType: "stop-capture",
        targetFrame: targetFrame,
        targetTrack: targetTrack
      }, "*");
    const message = await waiter;
    return message.result;
  }

  if (targetTrack == "original") {
    track.stop();
    track = undefined;
  } else if (targetTrack == "clone") {
    trackClone.stop();
    trackClone = undefined;
  } else if (targetTrack == "second") {
    otherCaptureTrack.stop();
    otherCaptureTrack = undefined
  } else {
    return "error";
  }

  return `${role}-stop-success`;
}

async function cropTargetFromElement(targetFrame, elementId) {
  if (role == targetFrame) {
    try {
      const element = document.getElementById(elementId);
      return makeCropTargetProxy(await CropTarget.fromElement(element));
    } catch (e) {
      return `${role}-produce-crop-target-error`;
    }
  } else {
    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("produce-crop-target-complete");
    embedded_frame.contentWindow.postMessage({
        messageType: "produce-crop-target",
        element: elementId
      }, "*");
    const message = await waiter;
    return message.result;
  }
}

async function cropTo(cropTarget, targetFrame, targetTrackStr) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      return "error";
    }

    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("crop-to-complete");
    embedded_frame.contentWindow.postMessage(
        {
          messageType: 'crop-to',
          cropTarget: cropTarget,
          targetFrame: targetFrame,
          targetTrackStr: targetTrackStr
        },
        '*');
    const message = await waiter;
    return message.result;
  }

  const targetTrack = (targetTrackStr == "original" ? track :
                       targetTrackStr == "clone" ? trackClone :
                       targetTrackStr == "second" ? otherCaptureTrack :
                       undefined);
  if (!targetTrack) {
    return "error";
  }

  try {
    await targetTrack.cropTo(cropTarget);
    return `${role}-crop-success`;
  } catch (e) {
    return `${role}-crop-error`;
  }
}

////////////////////////////////////////////////////////////
// Communication with other pages belonging to this page. //
////////////////////////////////////////////////////////////

async function setUpMailman(url) {
  const mailman_frame = document.getElementById("mailman_frame");
  mailman_frame.src = url;

  window.addEventListener("message", (event) => {
    if (event.data.messageType == "announce-crop-target") {
      registerRemoteCropTarget(event.data.cropTarget, event.data.index);
    }
  });

  await waitForMessage('mailman-embedded');
  if (role == "top-level") {
    const waiter = waitForMessage('mailman-ready');
    document.getElementById("embedded_frame").contentWindow.postMessage({
        messageType: "setup-mailman",
        url: url
      }, "*");
    const message = await waiter;
    return message.messageType;
  }
  return {messageType: "mailman-ready"};
}

function waitForMessage(expectedMessageType) {
  return new Promise(resolve => {
    const listener = (event) => {
      if (event.data.messageType == expectedMessageType) {
        window.removeEventListener("message", listener);
        resolve(event.data);
      }
    };
    window.addEventListener("message", listener);
  });
}

function broadcast(msg) {
  const mailman_frame = document.getElementById("mailman_frame");
  mailman_frame.contentWindow.postMessage(msg, "*");
}


//////////////////////////////////////////////////////////////////
// Maintenance of a shared CropTarget database so that any page //
// could be tested in cropping to a target in any other page.   //
//////////////////////////////////////////////////////////////////

// Because CropTargets are not stringifiable, we cache them here and give
// the C++ test fixture their index. When the C++ test fixture hands back
// the index as input to an action, we use the relevant CropTarget stored here.
//
// When a page mints a CropTarget, it propagates it to all other pages.
// Each page acks it back, and when the minter receives the expected
// number of acks, it returns control to the C++ fixture.

// Contains all of the CropTargets this page knows of.
const cropTargets = [];

const EXPECTED_ACKS = 3;

async function makeCropTargetProxy(cropTarget) {
  cropTargets.push(cropTarget);

  let ackCount = 0;
  let expectedAckIndex = cropTargets.length - 1;

  const acksCompletedPromise = new Promise(resolve => {
    let ackListener = (event) => {
      if (event.data.messageType == "ack-crop-target") {
        if (event.data.index != expectedAckIndex) {
          return;
        }

        if (++ackCount != EXPECTED_ACKS) {
          return;
        }

        window.removeEventListener("message", ackListener);
        resolve(`${cropTargets.length - 1}`);
      }
    };
    window.addEventListener("message", ackListener);
  });

  broadcast({
    messageType: "announce-crop-target",
    cropTarget: cropTarget,
    index: expectedAckIndex
  });

  return acksCompletedPromise;
}

function getCropTarget(cropTargetIndex) {
  return cropTargets[cropTargetIndex];
}


function registerRemoteCropTarget(cropTarget, index) {
  cropTargets.push(cropTarget);
  broadcast({messageType: "ack-crop-target", index: index});
}
