// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var getURL = chrome.extension.getURL;
var deepEq = chrome.test.checkDeepEq;
var expectedEventData;
var capturedEventData;
var capturedUnexpectedData;
var expectedEventOrder;
var tabId;
var tabIdMap;
var frameIdMap;
var testWebSocketPort;
var testServerPort;
var testServer = "www.a.com";
var defaultScheme = "http";
var eventsCaptured;
var listeners = {
  'onBeforeRequest': [],
  'onBeforeSendHeaders': [],
  'onAuthRequired': [],
  'onSendHeaders': [],
  'onHeadersReceived': [],
  'onResponseStarted': [],
  'onBeforeRedirect': [],
  'onCompleted': [],
  'onErrorOccurred': []
};
// Requests initiated by a user interaction with the browser is a
// BROWSER_INITIATED action. If the request was instead initiated by the tabs
// extension API, a website or code run in the context of a website then it's
// WEB_INITIATED.
const initiators = {
  BROWSER_INITIATED: 2,
  WEB_INITIATED: 3
};

// If true, don't bark on events that were not registered via expect().
// These events are recorded in capturedUnexpectedData instead of
// capturedEventData.
var ignoreUnexpected = false;

// This is a debugging aid to print all received events as well as the
// information whether they were expected.
var debug = false;

// Runs the |tests| using the |tab| as a default tab.
function runTestsForTab(tests, tab) {
  tabId = tab.id;
  tabIdMap = {"-1": -1};
  tabIdMap[tabId] = 0;
  chrome.test.getConfig(function(config) {
    testServerPort = config.testServer.port;
    testWebSocketPort = config.testWebSocketPort;
    chrome.test.runTests(tests);
  });
}

// Creates an "about:blank" tab and runs |tests| with this tab as default.
function runTests(tests) {
  chrome.test.getConfig(function(config) {
    if (config.customArg == 'debug')
      debug = true;

    var waitForAboutBlank = function(_, info, tab) {
      if (debug) {
        console.log("tabs.OnUpdated received in waitForAboutBlank: " +
          JSON.stringify(info) + " " + JSON.stringify(tab));
      }
      if (info.status == "complete" && tab.url == "about:blank") {
        chrome.tabs.onUpdated.removeListener(waitForAboutBlank);
        runTestsForTab(tests, tab);
      }
    };

    chrome.tabs.onUpdated.addListener(waitForAboutBlank);
    chrome.tabs.create({url: "about:blank"});
  });
}

// Returns an URL from the test server, fixing up the port. Must be called
// from within a test case passed to runTests.
function getServerURL(opt_path, opt_host, opt_scheme) {
  if (!testServerPort)
    throw new Error("Called getServerURL outside of runTests.");
  var host = opt_host || testServer;
  var scheme = opt_scheme || defaultScheme;
  var path = opt_path || '';
  return scheme + "://" + host + ":" + testServerPort + "/" + path;
}

// Throws error if an invalid navigation type was presented.
function validateNavigationType(navigationType) {
  if (navigationType == undefined)
    throw new Error("A navigation type must be defined.");
  if(Object.values(initiators).indexOf(navigationType) === -1)
    throw new Error("Unknown navigation type.");
}

// Similar to getURL without the path. The |navigationType| specifies if the
// navigation was performed by the browser or the renderer. A browser initiated
// navigation doesn't have an initiator.
function getDomain(navigationType) {
  validateNavigationType(navigationType);
  if (navigationType == initiators.BROWSER_INITIATED)
    return undefined;
  else
    return getURL('').slice(0,-1);
}

// Similar to getServerURL without the path. The |navigationType| specifies if
// the navigation was performed by the browser or the renderer. A browser
// initiated navigation doesn't have an initiator.
function getServerDomain(navigationType, opt_host, opt_scheme) {
  validateNavigationType(navigationType);
  if (navigationType == initiators.BROWSER_INITIATED)
    return undefined;
  else
    return getServerURL(undefined, opt_host, opt_scheme).slice(0, -1);
}

// Helper to advance to the next test only when the tab has finished loading.
// This is because tabs.update can sometimes fail if the tab is in the middle
// of a navigation (from the previous test), resulting in flakiness.
function navigateAndWait(url, callback) {
  var done = chrome.test.listenForever(chrome.tabs.onUpdated,
      function (_, info, tab) {
    if (debug) {
      console.log("tabs.OnUpdated received in navigateAndWait: " +
        JSON.stringify(info) + " " + JSON.stringify(tab));
    }
    if (tab.id == tabId && info.status == "complete") {
      if (callback) callback(tab);
      done();
    }
  });
  chrome.test.sendMessage(JSON.stringify({navigate: {tabId: tabId, url: url}}));
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
//       event: "<webrequest event type>",
//       details: { <expected details of the webrequest event> },
//       retval: { <dictionary that the event handler shall return> } (optional)
//       retval_function: <function to run when the event occurs, this overrides
//                         any retval handling. The function takes
//                         (name, details, optional callback). The value it
//                         returns is returned out of the event
//                         handler> (optional)
//     }
// order: an array of sequences, e.g. [ ["a", "b", "c"], ["d", "e"] ] means that
//     event with label "a" needs to occur before event with label "b". The
//     relative order of "a" and "d" does not matter.
// filter: filter dictionary passed on to the event subscription of the
//     webRequest API.
// extraInfoSpec: the union of all desired extraInfoSpecs for the events.
function expect(data, order, filter, extraInfoSpec) {
  expectedEventData = data || [];
  capturedEventData = [];
  capturedUnexpectedData = [];
  expectedEventOrder = order || [];

  if (expectedEventData.length > 0) {
    eventsCaptured = chrome.test.callbackAdded();
  }
  tabAndFrameUrls = {};  // Maps "{tabId}-{frameId}" to the URL of the frame.
  frameIdMap = {"-1": -1, "0": 0};
  removeListeners();
  resetDeclarativeRules();
  initListeners(filter || {urls: ["<all_urls>"]}, extraInfoSpec || []);
  // Fill in default values.
  for (var i = 0; i < expectedEventData.length; ++i) {
    if (!('method' in expectedEventData[i].details)) {
      expectedEventData[i].details.method = "GET";
    }
    if (!('tabId' in expectedEventData[i].details)) {
      expectedEventData[i].details.tabId = tabIdMap[tabId];
    }
    if (!('frameId' in expectedEventData[i].details)) {
      expectedEventData[i].details.frameId = 0;
    }
    if (!('parentFrameId' in expectedEventData[i].details)) {
      expectedEventData[i].details.parentFrameId = -1;
    }
    if (!('type' in expectedEventData[i].details)) {
      expectedEventData[i].details.type = "main_frame";
    }
    if ('initiator' in expectedEventData[i].details &&
        expectedEventData[i].details.initiator == undefined) {
      delete expectedEventData[i].details.initiator;
    }
  }
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  if (capturedEventData.length > expectedEventData.length) {
    chrome.test.fail("Recorded too many events. " +
        JSON.stringify(capturedEventData));
    return;
  }
  // We have ensured that capturedEventData contains exactly the same elements
  // as expectedEventData. Now we need to verify the ordering.
  // Step 1: build positions such that
  //     positions[<event-label>]=<position of this event in capturedEventData>
  var curPos = 0;
  var positions = {}
  capturedEventData.forEach(function (event) {
    chrome.test.assertTrue(event.hasOwnProperty("label"));
    positions[event.label] = curPos;
    curPos++;
  });
  // Step 2: check that elements arrived in correct order
  expectedEventOrder.forEach(function (order) {
    var previousLabel = undefined;
    order.forEach(function(label) {
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

  removeListeners();
  eventsCaptured();
}

// Simple check to see that we have a User-Agent header, and that it contains
// an expected value. This is a basic check that the request headers are valid.
function checkUserAgent(headers) {
  for (var i in headers) {
    if (headers[i].name.toLowerCase() == "user-agent")
      return headers[i].value.toLowerCase().indexOf("chrome") != -1;
  }
  return false;
}

// Whether the request is missing a tabId and frameId and we're not expecting
// a request with the given details. If the method returns true, the event
// should be ignored.
function isUnexpectedDetachedRequest(name, details) {
  // This function is responsible for marking detached requests as unexpected.
  // Non-detached requests are not this function's concern.
  if (details.tabId !== -1 || details.frameId >= 0)
    return false;

  // Only return true if there is no matching expectation for the given details.
  return !expectedEventData.some(function(exp) {
    var didMatchTabAndFrameId =
      exp.details.tabId === -1 &&
      exp.details.frameId === -1;

    // Accept non-matching tabId/frameId for ping/beacon requests because these
    // requests can continue after a frame is removed. And due to a bug, such
    // requests have a tabId/frameId of -1.
    // The test will fail anyway, but then with a helpful error (expectation
    // differs from actual events) instead of an obscure test timeout.
    // TODO(robwu): Remove this once https://crbug.com/522129 gets fixed.
    didMatchTabAndFrameId = didMatchTabAndFrameId || details.type === 'ping';

    return name === exp.event &&
      didMatchTabAndFrameId &&
      exp.details.method === details.method &&
      exp.details.url === details.url &&
      exp.details.type === details.type;
  });
}

function captureEvent(name, details, callback) {
  // Ignore system-level requests like safebrowsing updates and favicon fetches
  // since they are unpredictable.
  if ((details.type == "other" && !details.url.includes('dont-ignore-me')) ||
      isUnexpectedDetachedRequest(name, details) ||
      details.url.match(/\/favicon.ico$/) ||
      details.url.match(/https:\/\/dl.google.com/)) {
    return;
  }

  var originalDetails = deepCopy(details);

  // Check that the frameId can be used to reliably determine the URL of the
  // frame that caused requests.
  if (name == "onBeforeRequest") {
    chrome.test.assertTrue('frameId' in details &&
                           typeof details.frameId === 'number');
    chrome.test.assertTrue('tabId' in details &&
                            typeof details.tabId === 'number');
    var key = details.tabId + "-" + details.frameId;
    if (details.type == "main_frame" || details.type == "sub_frame") {
      tabAndFrameUrls[key] = details.url;
    }
    details.frameUrl = tabAndFrameUrls[key] || "unknown frame URL";
  }

  // This assigns unique IDs to frames. The new IDs are only deterministic, if
  // the frames documents are loaded in order. Don't write browser tests with
  // more than one frame ID and rely on their numbers.
  if (!(details.frameId in frameIdMap)) {
    // Subtract one to discount for {"-1": -1} mapping that always exists.
    // This gives the first frame the ID 0.
    frameIdMap[details.frameId] = Object.keys(frameIdMap).length - 1;
  }
  details.frameId = frameIdMap[details.frameId];
  details.parentFrameId = frameIdMap[details.parentFrameId];

  // This assigns unique IDs to newly opened tabs. However, the new IDs are only
  // deterministic, if the order in which the tabs are opened is deterministic.
  if (!(details.tabId in tabIdMap)) {
    // Subtract one because the map is initialized with {"-1": -1}, and the
    // first tab has ID 0.
    tabIdMap[details.tabId] = Object.keys(tabIdMap).length - 1;
  }
  details.tabId = tabIdMap[details.tabId];

  delete details.requestId;
  delete details.timeStamp;
  if (details.requestHeaders) {
    details.requestHeadersValid = checkUserAgent(details.requestHeaders);
    delete details.requestHeaders;
  }
  if (details.responseHeaders) {
    details.responseHeadersExist = true;
    delete details.responseHeaders;
  }

  // Check if the equivalent event is already captured, and issue a unique
  // |eventCount| to identify each.
  var eventCount = 0;
  capturedEventData.forEach(function (event) {
    if (deepEq(event.event, name) && deepEq(event.details, details)) {
      eventCount++;
      // update |details| for the next match.
      details.eventCount = eventCount;
    }
  });

  // find |details| in matchingExpectedEventData
  var matchingExpectedEvent = undefined;
  expectedEventData.forEach(function (exp) {
    if (deepEq(exp.event, name) && deepEq(exp.details, details)) {
      if (matchingExpectedEvent) {
        chrome.test.fail("Duplicated expectation entry '" + exp.label +
        "' should be identified by |eventCount|: " + JSON.stringify(details));
      } else {
        matchingExpectedEvent = exp;
      }
    }
  });
  if (!matchingExpectedEvent && !ignoreUnexpected) {
    console.log("Expected events: " +
        JSON.stringify(expectedEventData, null, 2));
    chrome.test.fail("Received unexpected event '" + name + "':" +
        JSON.stringify(details, null, 2));
  }

  var retval;
  var retval_function;
  if (matchingExpectedEvent) {
    if (debug) {
      console.log("Expected event received: " + name + ": " +
        JSON.stringify(details));
    }
    capturedEventData.push(
        {label: matchingExpectedEvent.label, event: name, details: details});

    // checkExpecations decrements the counter of pending events. We may only
    // call it if an expected event has occurred.
    checkExpectations();

    // Pull the extra per-event options out of the expected data. These let us
    // specify special return values per event.
    retval = matchingExpectedEvent.retval;
    retval_function = matchingExpectedEvent.retval_function;
  } else {
    if (debug) {
      console.log('NOT Expected event received: ' + name + ': ' +
        JSON.stringify(details));
    }
    capturedUnexpectedData.push({event: name, details: details});
  }

  if (retval_function) {
    return retval_function(name, originalDetails, callback);
  }

  if (callback) {
    window.setTimeout(callback, 0, retval);
  } else {
    return retval;
  }
}

// Simple array intersection. We use this to filter extraInfoSpec so
// that only the allowed specs are sent to each listener.
function intersect(array1, array2) {
  return array1.filter(function(x) { return array2.indexOf(x) != -1; });
}

function initListeners(filter, extraInfoSpec) {
  var onBeforeRequest = function(details) {
    return captureEvent("onBeforeRequest", details);
  };
  listeners['onBeforeRequest'].push(onBeforeRequest);

  var onBeforeSendHeaders = function(details) {
    return captureEvent("onBeforeSendHeaders", details);
  };
  listeners['onBeforeSendHeaders'].push(onBeforeSendHeaders);

  var onSendHeaders = function(details) {
    return captureEvent("onSendHeaders", details);
  };
  listeners['onSendHeaders'].push(onSendHeaders);

  var onHeadersReceived = function(details) {
    return captureEvent("onHeadersReceived", details);
  };
  listeners['onHeadersReceived'].push(onHeadersReceived);

  var onAuthRequired = function(details, callback) {
    return captureEvent("onAuthRequired", details, callback);
  };
  listeners['onAuthRequired'].push(onAuthRequired);

  var onResponseStarted = function(details) {
    return captureEvent("onResponseStarted", details);
  };
  listeners['onResponseStarted'].push(onResponseStarted);

  var onBeforeRedirect = function(details) {
    return captureEvent("onBeforeRedirect", details);
  };
  listeners['onBeforeRedirect'].push(onBeforeRedirect);

  var onCompleted = function(details) {
    return captureEvent("onCompleted", details);
  };
  listeners['onCompleted'].push(onCompleted);

  var onErrorOccurred = function(details) {
    return captureEvent("onErrorOccurred", details);
  };
  listeners['onErrorOccurred'].push(onErrorOccurred);

  chrome.webRequest.onBeforeRequest.addListener(
      onBeforeRequest, filter,
      intersect(extraInfoSpec, ['blocking', 'requestBody']));

  chrome.webRequest.onBeforeSendHeaders.addListener(
      onBeforeSendHeaders, filter,
      intersect(extraInfoSpec, ['blocking', 'requestHeaders', 'extraHeaders']));

  chrome.webRequest.onSendHeaders.addListener(
      onSendHeaders, filter,
      intersect(extraInfoSpec, ['requestHeaders', 'extraHeaders']));

  chrome.webRequest.onHeadersReceived.addListener(
      onHeadersReceived, filter,
      intersect(extraInfoSpec, ['blocking', 'responseHeaders',
                                'extraHeaders']));

  chrome.webRequest.onAuthRequired.addListener(
      onAuthRequired, filter,
      intersect(extraInfoSpec, ['asyncBlocking', 'blocking',
                                'responseHeaders', 'extraHeaders']));

  chrome.webRequest.onResponseStarted.addListener(
      onResponseStarted, filter,
      intersect(extraInfoSpec, ['responseHeaders', 'extraHeaders']));

  chrome.webRequest.onBeforeRedirect.addListener(
      onBeforeRedirect, filter, intersect(extraInfoSpec,
      ['responseHeaders','extraHeaders']));

  chrome.webRequest.onCompleted.addListener(
      onCompleted, filter,
      intersect(extraInfoSpec, ['responseHeaders', 'extraHeaders']));

  chrome.webRequest.onErrorOccurred.addListener(onErrorOccurred, filter);
}

function removeListeners() {
  function helper(eventName) {
    for (var i in listeners[eventName]) {
      chrome.webRequest[eventName].removeListener(listeners[eventName][i]);
    }
    listeners[eventName].length = 0;
    chrome.test.assertFalse(chrome.webRequest[eventName].hasListeners());
  }
  helper('onBeforeRequest');
  helper('onBeforeSendHeaders');
  helper('onAuthRequired');
  helper('onSendHeaders');
  helper('onHeadersReceived');
  helper('onResponseStarted');
  helper('onBeforeRedirect');
  helper('onCompleted');
  helper('onErrorOccurred');
}

function resetDeclarativeRules() {
  chrome.declarativeWebRequest.onRequest.removeRules();
}

function checkHeaders(headers, requiredNames, disallowedNames) {
  var headerMap = {};
  for (var i = 0; i < headers.length; i++)
    headerMap[headers[i].name.toLowerCase()] = headers[i].value;

  for (var i = 0; i < requiredNames.length; i++) {
    chrome.test.assertTrue(!!headerMap[requiredNames[i]],
        'Missing header: ' + requiredNames[i]);
  }
  for (var i = 0; i < disallowedNames.length; i++) {
    chrome.test.assertFalse(!!headerMap[disallowedNames[i]],
        'Header should not be present: ' + disallowedNames[i]);
  }
}

function removeHeader(headers, name) {
  for (var i = 0; i < headers.length; i++) {
    if (headers[i].name.toLowerCase() == name) {
      headers.splice(i, 1);
      break;
    }
  }
}
