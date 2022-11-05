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

function startEmbeddingFrame(url) {
  const embedded_frame = document.getElementById("embedded_frame");
  embedded_frame.src = url;
  // window.domAutomationController.send() called by embedded page.
}

function registerEmbeddedListeners() {
  window.addEventListener("message", (event) => {
    const type = event.data.messageType;
    if (type == "setup-mailman") {
      setUpMailman(event.data.url);
    } else if (type == "start-capture") {
      startCapture();
    } else if (type == "produce-crop-target") {
      cropTargetFromElement("embedded", event.data.element);
    } else if (type == "crop-to") {
      cropTo(event.data.cropTarget, event.data.targetFrame,
             event.data.targetTrackStr);
    } else if (type == 'create-new-element') {
      createNewElement(event.data.targetFrame, event.data.tag, event.data.id);
    } else if (type == 'start-second-capture') {
      startSecondCapture(event.data.targetFrame);
    } else if (type == 'stop-capture') {
      stopCapture(event.data.targetFrame, event.data.targetTrack);
    }
  });
}

// Allows creating new elements for which new crop-targets may be created.
function createNewElement(targetFrame, tag, id) {
  if (role == targetFrame) {
    const newElement = document.createElement(tag);
    newElement.id = id;
    document.body.appendChild(newElement);
    window.domAutomationController.send(`${role}-new-element-success`);
  } else {
    if (role != "top-level") {
      window.domAutomationController.send(`${role}-new-element-error`)
      return;
    }
    const embedded_frame = document.getElementById("embedded_frame");
    embedded_frame.contentWindow.postMessage(
        {
          messageType: 'create-new-element',
          targetFrame: targetFrame,
          tag: tag,
          id: id
        },
        '*');
    // window.domAutomationController.send() called by embedded page.
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
    window.domAutomationController.send("error-multiple-captures");
    return;
  }

  try {
    const stream = await navigator.mediaDevices.getDisplayMedia({
      video: true,
      selfBrowserSurface: "include"
    });
    [track] = stream.getVideoTracks();
    window.domAutomationController.send(`${role}-capture-success`);
  } catch (e) {
    window.domAutomationController.send(`${role}-capture-failure`);
  }
}

async function startSecondCapture(targetFrame) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      window.domAutomationController.send("error");
      return;
    }

    const embedded_frame = document.getElementById("embedded_frame");
    embedded_frame.contentWindow.postMessage({
        messageType: "start-second-capture",
        targetFrame: targetFrame
      }, "*");
    return;
    // window.domAutomationController.send() called by embedded page.
  }

  try {
    const stream = await navigator.mediaDevices.getDisplayMedia({
      video: true,
      selfBrowserSurface: "include"
    });
    [otherCaptureTrack] = stream.getVideoTracks();
    window.domAutomationController.send(`${role}-second-capture-success`);
  } catch (e) {
    window.domAutomationController.send(`${role}-second-capture-failure`);
  }
}

function stopCapture(targetFrame, targetTrack) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      window.domAutomationController.send("error");
      return;
    }

    const embedded_frame = document.getElementById("embedded_frame");
    embedded_frame.contentWindow.postMessage({
        messageType: "stop-capture",
        targetFrame: targetFrame,
        targetTrack: targetTrack
      }, "*");
    return;
    // window.domAutomationController.send() called by embedded page.
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
    window.domAutomationController.send("error");
    return;
  }

  window.domAutomationController.send(`${role}-stop-success`);
}

async function cropTargetFromElement(targetFrame, elementId) {
  if (role == targetFrame) {
    try {
      const element = document.getElementById(elementId);
      makeCropTargetProxy(await CropTarget.fromElement(element));
      // window.domAutomationController.send() called after
      // the CropTarget is propagated to all participating pages.
    } catch (e) {
      window.domAutomationController.send(`${role}-produce-crop-target-error`);
    }
  } else {
    const embedded_frame = document.getElementById("embedded_frame");
    embedded_frame.contentWindow.postMessage({
        messageType: "produce-crop-target",
        element: elementId
      }, "*");
    // window.domAutomationController.send() called by embedded page.
  }
}

async function cropTo(cropTarget, targetFrame, targetTrackStr) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      window.domAutomationController.send("error");
      return;
    }

    const embedded_frame = document.getElementById("embedded_frame");
    embedded_frame.contentWindow.postMessage(
        {
          messageType: 'crop-to',
          cropTarget: cropTarget,
          targetFrame: targetFrame,
          targetTrackStr: targetTrackStr
        },
        '*');
    // window.domAutomationController.send() called by embedded page.
    return;
  }

  const targetTrack = (targetTrackStr == "original" ? track :
                       targetTrackStr == "clone" ? trackClone :
                       targetTrackStr == "second" ? otherCaptureTrack :
                       undefined);
  if (!targetTrack) {
    window.domAutomationController.send("error");
    return;
  }

  try {
    await targetTrack.cropTo(cropTarget);
    window.domAutomationController.send(`${role}-crop-success`);
  } catch (e) {
    window.domAutomationController.send(`${role}-crop-error`);
  }
}

////////////////////////////////////////////////////////////
// Communication with other pages belonging to this page. //
////////////////////////////////////////////////////////////

function setUpMailman(url) {
  const mailman_frame = document.getElementById("mailman_frame");
  mailman_frame.src = url;

  window.addEventListener("message", (event) => {
    if (event.data.messageType == "mailman-embedded") {
      if (role == "top-level") {
        const embedded_frame = document.getElementById("embedded_frame");
        embedded_frame.contentWindow.postMessage({
            messageType: "setup-mailman",
            url: url
          }, "*");
        // window.domAutomationController.send() called by embedded page.
      } else {
        window.domAutomationController.send("mailman-ready");
      }
    } else if (event.data.messageType == "announce-crop-target") {
      registerRemoteCropTarget(event.data.cropTarget, event.data.index);
    } else if (event.data.messageType == "ack-crop-target") {
      onAckReceived(event.data.index);
    }
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
let ackCount;
let expectedAckIndex;

function makeCropTargetProxy(cropTarget) {
  cropTargets.push(cropTarget);

  ackCount = 0;
  expectedAckIndex = cropTargets.length - 1;

  broadcast({
    messageType: "announce-crop-target",
    cropTarget: cropTarget,
    index: expectedAckIndex
  });
}

function getCropTarget(cropTargetIndex) {
  return cropTargets[cropTargetIndex];
}


function registerRemoteCropTarget(cropTarget, index) {
  cropTargets.push(cropTarget);
  broadcast({messageType: "ack-crop-target", index: index});
}

function onAckReceived(index) {
  if (index != expectedAckIndex) {
    return;
  }

  if (++ackCount != EXPECTED_ACKS) {
    return;
  }

  ackCount = undefined;
  expectedAckIndex = undefined;
  window.domAutomationController.send(`${cropTargets.length - 1}`);
}
