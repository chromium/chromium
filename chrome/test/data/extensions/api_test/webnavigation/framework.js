// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deepEq = chrome.test.checkDeepEq;
var expectedEventData;
var expectedEventOrder;
var capturedEventData;
var nextFrameId;
var frameIds;
var nextTabId;
var tabIds;
var nextProcessId;
var processIds;
var initialized = false;

var debug = false;

// Helper function. Turns a function returning an object in a callback into a
// promise. It helps keeping the code at the same indentation level.
function promise(fun, ...args) {
  return new Promise(function(resolve, reject) {
    fun(...args, function(value) {
      resolve(value);
    });
  });
}

function deepCopy(obj) {
  if (obj === null)
    return null;
  if (typeof(obj) != 'object')
    return obj;
  if (Array.isArray(obj)) {
    var tmp_array = new Array;
    for (var i = 0; i < obj.length; i++) {
      tmp_array.push(deepCopy(obj[i]));
    }
    return tmp_array;
  }

  var tmp_object = {}
  for (var p in obj) {
    tmp_object[p] = deepCopy(obj[p]);
  }
  return tmp_object;
}

// data: array of expected events, each one is a dictionary:
//     { label: "<unique identifier>",
//       event: "<webnavigation event type>",
//       details: { <expected details of the event> }
//     }
// order: an array of sequences, e.g. [ ["a", "b", "c"], ["d", "e"] ] means that
//     event with label "a" needs to occur before event with label "b". The
//     relative order of "a" and "d" does not matter.
function expect(data, order) {
  expectedEventData = data;
  capturedEventData = [];
  expectedEventOrder = order;
  nextFrameId = 1;
  frameIds = {};
  nextTabId = 0;
  tabIds = {};
  nextProcessId = -1;
  processIds = {}
  initListeners();
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  if (capturedEventData.length > expectedEventData.length) {
    chrome.test.fail("Recorded too many events. " +
        JSON.stringify(capturedEventData));
  }
  // We have ensured that capturedEventData contains exactly the same elements
  // as expectedEventData. Now we need to verify the ordering.
  // Step 1: build positions such that
  //     position[<event-label>]=<position of this event in capturedEventData>
  var curPos = 0;
  var positions = {};
  capturedEventData.forEach(function (event) {
    chrome.test.assertTrue(event.hasOwnProperty("label"));
    positions[event.label] = curPos;
    curPos++;
  });
  // Step 2: check that elements arrived in correct order
  expectedEventOrder.forEach(function (order) {
    var previousLabel = undefined;
    order.forEach(function (label) {
      if (previousLabel === undefined) {
        previousLabel = label;
        return;
      }
      chrome.test.assertTrue(positions[previousLabel] < positions[label],
          "Event " + previousLabel + " is supposed to arrive before " +
          label + ".");
      previousLabel = label;
    });
  });
  chrome.test.succeed();
}

function captureEvent(name, details) {
  if ('url' in details) {
    // Skip about:blank navigations
    if (details.url == 'about:blank') {
      return;
    }
    // Strip query parameter as it is hard to predict.
    details.url = details.url.replace(new RegExp('\\?[^#]*'), '');
  }
  // normalize details.
  if ('timeStamp' in details) {
    details.timeStamp = 0;
  }
  if (('frameId' in details) && (details.frameId != 0)) {
    if (frameIds[details.frameId] === undefined) {
      frameIds[details.frameId] = nextFrameId++;
    }
    details.frameId = frameIds[details.frameId];
  }
  if (('parentFrameId' in details) && (details.parentFrameId > 0)) {
    if (frameIds[details.parentFrameId] === undefined) {
      frameIds[details.parentFrameId] = nextFrameId++;
    }
    details.parentFrameId = frameIds[details.parentFrameId];
  }
  if (('sourceFrameId' in details) && (details.sourceFrameId != 0)) {
    if (frameIds[details.sourceFrameId] === undefined) {
      frameIds[details.sourceFrameId] = nextFrameId++;
    }
    details.sourceFrameId = frameIds[details.sourceFrameId];
  }
  if ('tabId' in details) {
    if (tabIds[details.tabId] === undefined) {
      tabIds[details.tabId] = nextTabId++;
    }
    details.tabId = tabIds[details.tabId];
  }
  if ('sourceTabId' in details) {
    if (tabIds[details.sourceTabId] === undefined) {
      tabIds[details.sourceTabId] = nextTabId++;
    }
    details.sourceTabId = tabIds[details.sourceTabId];
  }
  if ('replacedTabId' in details) {
    if (tabIds[details.replacedTabId] === undefined) {
      tabIds[details.replacedTabId] = nextTabId++;
    }
    details.replacedTabId = tabIds[details.replacedTabId];
  }
  if ('processId' in details) {
    if (processIds[details.processId] === undefined) {
      processIds[details.processId] = nextProcessId++;
    }
    details.processId = processIds[details.processId];
  }
  if ('sourceProcessId' in details) {
    if (processIds[details.sourceProcessId] === undefined) {
      processIds[details.sourceProcessId] = nextProcessId++;
    }
    details.sourceProcessId = processIds[details.sourceProcessId];
  }

  if (debug)
    console.log("Received event '" + name + "':" + JSON.stringify(details));

  // find |details| in expectedEventData
  var found = false;
  var label = undefined;
  expectedEventData.forEach(function (exp) {
    if (exp.event == name) {
      var exp_details;
      var alt_details;
      if ('transitionQualifiers' in exp.details) {
        var idx = exp.details['transitionQualifiers'].indexOf(
            'maybe_client_redirect');
        if (idx >= 0) {
          exp_details = deepCopy(exp.details);
          exp_details['transitionQualifiers'].splice(idx, 1);
          alt_details = deepCopy(exp_details);
          alt_details['transitionQualifiers'].push('client_redirect');
        } else {
          exp_details = exp.details;
          alt_details = exp.details;
        }
      } else {
        exp_details = exp.details;
        alt_details = exp.details;
      }
      if (deepEq(exp_details, details) || deepEq(alt_details, details)) {
        if (!found) {
          found = true;
          label = exp.label;
          exp.event = undefined;
        }
      }
    }
  });
  if (!found) {
    chrome.test.fail("Received unexpected event '" + name + "':" +
        JSON.stringify(details));
  }
  capturedEventData.push({label: label, event: name, details: details});
  checkExpectations();
}

function initListeners() {
  if (initialized)
    return;
  initialized = true;
  chrome.webNavigation.onBeforeNavigate.addListener(
      function(details) {
    captureEvent("onBeforeNavigate", details);
  });
  chrome.webNavigation.onCommitted.addListener(
      function(details) {
    captureEvent("onCommitted", details);
  });
  chrome.webNavigation.onDOMContentLoaded.addListener(
      function(details) {
    captureEvent("onDOMContentLoaded", details);
  });
  chrome.webNavigation.onCompleted.addListener(
      function(details) {
    captureEvent("onCompleted", details);
  });
  chrome.webNavigation.onCreatedNavigationTarget.addListener(
      function(details) {
    captureEvent("onCreatedNavigationTarget", details);
  });
  chrome.webNavigation.onReferenceFragmentUpdated.addListener(
      function(details) {
    captureEvent("onReferenceFragmentUpdated", details);
  });
  chrome.webNavigation.onErrorOccurred.addListener(
      function(details) {
    captureEvent("onErrorOccurred", details);
  });
  chrome.webNavigation.onTabReplaced.addListener(
      function(details) {
    captureEvent("onTabReplaced", details);
  });
  chrome.webNavigation.onHistoryStateUpdated.addListener(
      function(details) {
    captureEvent("onHistoryStateUpdated", details);
  });
}

// Returns the usual order of navigation events.
function navigationOrder(prefix) {
  return [ prefix + "onBeforeNavigate",
           prefix + "onCommitted",
           prefix + "onDOMContentLoaded",
           prefix + "onCompleted" ];
}

// Returns the constraints expressing that a frame is an iframe of another
// frame.
function isIFrameOf(iframe, main_frame) {
  return [ main_frame + "onCommitted",
           iframe + "onBeforeNavigate",
           iframe + "onCompleted",
           main_frame + "onCompleted" ];
}

// Returns the constraint expressing that a frame was loaded by another.
function isLoadedBy(target, source) {
  return [ source + "onDOMContentLoaded", target + "onBeforeNavigate"];
}
