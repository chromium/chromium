/**
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

"use strict";

///////////////////////////
// Test page management. //
///////////////////////////

let track;
let trackClone;
let otherCaptureTrack;

let role;
function setRole(value) {
  role = value;
}

// Given a URL, set embedded_frame's source to that URL.
// Return "embedding-done" after embedding is done.
async function startEmbeddingFrame(url) {
  const waiter = waitForMessage('embedding-done');
  document.getElementById("embedded_frame").src = url;
  const message = await waiter;
  return message.messageType;
}

// Called by the embedded pages participating in the test.
// Registers listeners for messages from the top-level page.
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
    } else if (type == "produce-sub-target") {
      subCaptureTargetFromElement(event.data.targetType, "embedded",
                                  event.data.element)
        .then((result) => {
          reply({messageType: "produce-sub-target-complete", result});
        });
    } else if (type == "sub-capture-application") {
      applySubCapture(event.data.target, event.data.targetType,
                      event.data.targetFrame, event.data.targetTrackStr)
        .then((result) => {
          reply({messageType: "sub-capture-application-complete", result});
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
//
// Parameters:
// * targetFrame: Either "top-level" or "embedded". Determines in which
//   page the element should be created.
// * tag: The type of element to be created.
// * id: The ID which the new element should be assigned.
//
// Returns "create-new-element-complete" once done.
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

// Produces visual updates in the page, ensuring new frames
// will be emitted and reducing test flakiness.
function animate(element) {
  const animationCallback = function() {
    element.innerHTML = parseInt(element.innerHTML) + 1;
  };
  setInterval(animationCallback, 20);
}

/////////////////////////////////////////
// Main actions from C++ test fixture. //
/////////////////////////////////////////

// Starts the main capture session.
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

// Starts a second capture, associated with `otherCaptureTrack`,
// in the indicated target-frame (top-level / embedded).
// Returns `${role}-second-capture-success` upon success,
// `${role}-second-capture-failure` if unsuccessful.
async function startSecondCapture(targetFrame) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      throw "error";
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

// Stops a specific capture in a specific frame.
// * targetFrame: Either "top-level" or "embedded".
// * targetTrack: "original", "clone" or "second", referencing
//   `track`, `trackClone` and `otherCaptureTrack`, respectively.
async function stopCapture(targetFrame, targetTrack) {
  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      throw "error";
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

// Produce a token of type `targetType` for the element
// whose ID is `elementId`.
async function mintToken(targetType, elementId) {
  const element = document.getElementById(elementId);
  if (!element) {
    throw "unknown-element-error";
  }

  if (targetType == "crop-target") {
    return CropTarget.fromElement(element);
  } else if (targetType == "restriction-target") {
    return RestrictionTarget.fromElement(element);
  } else {
    throw "unknown-target-type-error";
  }
}

// Produce a token for a given element in the indicated frame.
//
// Parameters:
// * targetType: The type of token (crop-target / restriction-target).
// * targetFrame: The frame (top-level / embedded).
// * elementId: The ID of the element for which the token should be produced.
//
// The token is stored in subCaptureTargets and its index is returned.
async function subCaptureTargetFromElement(targetType, targetFrame, elementId) {
  const resultPrefix = `${role}-produce-${targetType}`;
  if (role == targetFrame) {
    try {
      const token = await mintToken(targetType, elementId);
      return makeSubCaptureTargetProxy(token);
    } catch (e) {
      return resultPrefix + "-error";
    }
  } else {
    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("produce-sub-target-complete");
    embedded_frame.contentWindow.postMessage({
        messageType: "produce-sub-target",
        element: elementId,
        targetType
      }, "*");
    const message = await waiter;
    return message.result;
  }
}

// Applies sub-capture; that is, calls cropTo() or restrictTo().
//
// Parameters:
// * target: Indicates the target of cropping/restricting. (Note that this
//   might be `undefined`.)
// * targetType: Indicates which type of sub-target `target` represents.
//   This parameter is necessary when `target` is `undefined`; otherwise, it
//   is surmisable from the target.
// * targetFrame: Indicates the targeted frame (top-level / embedded).
// * targetTrackStr: Indicates the targeted track, in case the capturing
//   frame has multiple concurrent captures.
//
// Returns `${role}-${targetType}-success` upon success.
async function applySubCapture(target, targetType, targetFrame,
                               targetTrackStr) {
  // `targetType` is explicitly indicated for the case where `target`
  // is `undefined`.
  // Otherwise, the type can be derived from `target`'s actual type.
  const isValid =
    target == undefined ||
    (targetType == "crop-target" &&
      target.__proto__.constructor.name == "CropTarget") ||
    (targetType == "restriction-target" &&
      target.__proto__.constructor.name == "RestrictionTarget");
  if (!isValid) {
    throw "error-invalid-type";
  }


  if (targetFrame != `${role}`) {
    if (`${role}` != "top-level") {
      throw "unrecognized-target-frame-error";
    }

    const embedded_frame = document.getElementById("embedded_frame");
    const waiter = waitForMessage("sub-capture-application-complete");
    embedded_frame.contentWindow.postMessage(
        {
          messageType: 'sub-capture-application',
          target: target,
          targetType: targetType,
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
    throw "no-target-track-error";
  }

  try {
    if (targetType == "crop-target") {
      await targetTrack.cropTo(target);
    } else if (targetType == "restriction-target") {
      await targetTrack.restrictTo(target);
    } else {
      throw "unknown-type-error";
    }
    return `${role}-${targetType}-success`;
  } catch (e) {
    return `${role}-${targetType}-error`;
  }
}

////////////////////////////////////////////////////////////
// Communication with other pages belonging to this page. //
////////////////////////////////////////////////////////////

// To facilitate cross-origin communication, an embedded "mailman frame"
// is used. This "mailman frame" relays messages between (1) the frame which
// embeds the "mailman" and (2) the main frame of the other tab, which is
// same-origin with the mailman frame.
async function setUpMailman(url) {
  const mailman_frame = document.getElementById("mailman_frame");
  mailman_frame.src = url;

  window.addEventListener("message", (event) => {
    if (event.data.messageType == "announce-sub-capture-target") {
      subCaptureTargets.push(event.data.target);
      broadcast({
        messageType: "ack-sub-capture-target",
        index: event.data.index
      });
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

// Return a Promise that will remain pending until a specific event
// is received, then resolve with the data associated with
// that event's data field.
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

// Send a message to all other pages participating in this test.
function broadcast(msg) {
  const mailman_frame = document.getElementById("mailman_frame");
  mailman_frame.contentWindow.postMessage(msg, "*");
}


//////////////////////////////////////////////////////////////////
// Maintenance of a shared CropTarget database so that any page //
// could be tested in cropping to a target in any other page.   //
//////////////////////////////////////////////////////////////////

// Because CropTargets and RestrictionTargets are not stringifiable,
// we cache them here and give the C++ test fixture their index.
// When the C++ test fixture hands back the index as input to an action,
// we use the relevant token stored here.
//
// When a page mints a CropTarget/RestrictionTarget, it propagates it
// to all other pages. Each page acks it back, and when the minter
// receives the expected number of acks, it returns control to
// the C++ fixture.

// Contains all of the tokens this page knows of.
const subCaptureTargets = [];

const EXPECTED_ACKS = 3;

// Given a sub-capture token, announce it to all pages participating
// in the test, and return a promise which will resolve once all
// of those pages have acknowledged reception of the token.
async function makeSubCaptureTargetProxy(subCaptureTarget) {
  subCaptureTargets.push(subCaptureTarget);

  let ackCount = 0;
  let expectedAckIndex = subCaptureTargets.length - 1;

  const acksCompletedPromise = new Promise(resolve => {
    let ackListener = (event) => {
      if (event.data.messageType == "ack-sub-capture-target") {
        if (event.data.index != expectedAckIndex) {
          return;
        }

        if (++ackCount != EXPECTED_ACKS) {
          return;
        }

        window.removeEventListener("message", ackListener);
        resolve(`${subCaptureTargets.length - 1}`);
      }
    };
    window.addEventListener("message", ackListener);
  });

  broadcast({
    messageType: "announce-sub-capture-target",
    target: subCaptureTarget,
    index: expectedAckIndex
  });

  return acksCompletedPromise;
}

// The C++ fixture can only pass strings, not the actual JS objects.
// Therefore, whenever it wishes to refer to a specific sub-capture
// token, it references it via its index out of all sub-capture tokens
// produced by the test. This helper converts between the two.
// For convenience's sake, the special value `"undefined"` is allowed
// as well, and is converted to `undefined`.
function getSubCaptureTarget(subCaptureTargetIndex) {
  if (subCaptureTargetIndex == "undefined") {
    return undefined;
  }
  if (subCaptureTargetIndex >= subCaptureTargets.length) {
    throw "out-of-bounds-error";
  }
  return subCaptureTargets[subCaptureTargetIndex];
}
