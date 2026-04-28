// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TEST_SERVER = 'www.a.com';
const DEFAULT_SCHEME = 'http';

if ('ServiceWorkerGlobalScope' in self) {
  self.selfFrameId = -1;
} else {
  self.selfDocumentId = 1;
  self.selfFrameId = 0;
}

const getURL = chrome.runtime.getURL;
const deepEq = chrome.test.checkDeepEq;
let expectedEventData;
let capturedEventData;
let capturedUnexpectedData;
let expectedEventOrder;
let mparchEnabled;
let tabId;
let tabIdMap;
let documentIdMap;
let frameIdMap;
let testWebSocketPort;
let testWebTransportPort;
let testServerPort;
let eventsCaptured;
const listeners = {
  onBeforeRequest: [],
  onBeforeSendHeaders: [],
  onAuthRequired: [],
  onSendHeaders: [],
  onHeadersReceived: [],
  onResponseStarted: [],
  onBeforeRedirect: [],
  onCompleted: [],
  onErrorOccurred: [],
};
// Requests initiated by a user interaction with the browser is a
// BROWSER_INITIATED action. If the request was instead initiated by the tabs
// extension API, a website or code run in the context of a website then it's
// WEB_INITIATED.
const initiators = {
  BROWSER_INITIATED: 2,
  WEB_INITIATED: 3,
};

// If true, don't bark on events that were not registered via expect().
// These events are recorded in capturedUnexpectedData instead of
// capturedEventData.
// This is set by subtests, so we can't use `const` here.
let ignoreUnexpected = false;  // eslint-disable-line prefer-const

// This is a debugging aid to print all received events as well as the
// information whether they were expected.
let debug = false;

// Runs the |tests| using the |tab| as a default tab.
function runTestsForTab(tests, tab) {
  tabId = tab.id;
  tabIdMap = {'-1': -1};
  tabIdMap[tabId] = 0;
  chrome.test.getConfig(function(config) {
    testServerPort = config.testServer.port;
    testWebSocketPort = config.testWebSocketPort;
    testWebTransportPort = config.testWebTransportPort;
    chrome.test.runTests(tests);
  });
}

// Creates an "about:blank" tab and runs |tests| with this tab as default.
function runTests(tests) {
  chrome.test.getConfig(function(config) {
    if (config.customArg) {
      const args = JSON.parse(config.customArg);
      debug = args.debug;
      mparchEnabled = args.mparch;
      // Because the extension runs in split mode, only the incognito context
      // should run the tests.
      if (args.runInIncognito && !chrome.extension.inIncognitoContext) {
        return;
      }
    }

    const waitForAboutBlank = function(_, info, tab) {
      if (debug) {
        console.log(`tabs.OnUpdated received in waitForAboutBlank: ${
            JSON.stringify(info)} ${JSON.stringify(tab)}`);
      }
      if (info.status == 'complete' && tab.url == 'about:blank') {
        chrome.tabs.onUpdated.removeListener(waitForAboutBlank);
        runTestsForTab(tests, tab);
      }
    };

    chrome.tabs.onUpdated.addListener(waitForAboutBlank);
    chrome.tabs.create({url: 'about:blank'});
  });
}

// Returns an URL from the test server, fixing up the port. Must be called
// from within a test case passed to runTests.
function getServerURL(optPath, optHost, optScheme) {
  if (!testServerPort) {
    throw new Error('Called getServerURL outside of runTests.');
  }
  const host = optHost || TEST_SERVER;
  const scheme = optScheme || DEFAULT_SCHEME;
  const path = optPath || '';
  return `${scheme}://${host}:${testServerPort}/${path}`;
}

// Throws error if an invalid navigation type was presented.
function validateNavigationType(navigationType) {
  if (navigationType == undefined) {
    throw new Error('A navigation type must be defined.');
  }
  if (Object.values(initiators).indexOf(navigationType) === -1) {
    throw new Error('Unknown navigation type.');
  }
}

// Similar to getURL without the path. The |navigationType| specifies if the
// navigation was performed by the browser or the renderer. A browser initiated
// navigation doesn't have an initiator.
function getDomain(navigationType) {
  validateNavigationType(navigationType);
  if (navigationType == initiators.BROWSER_INITIATED) {
    return undefined;
  } else {
    return getURL('').slice(0, -1);
  }
}

// Similar to getServerURL without the path. The |navigationType| specifies if
// the navigation was performed by the browser or the renderer. A browser
// initiated navigation doesn't have an initiator.
function getServerDomain(navigationType, optHost, optScheme) {
  validateNavigationType(navigationType);
  if (navigationType == initiators.BROWSER_INITIATED) {
    return undefined;
  }
  return getServerURL(undefined, optHost, optScheme).slice(0, -1);
}

// Helper to advance to the next test only when the tab has finished loading.
// This is because tabs.update can sometimes fail if the tab is in the middle
// of a navigation (from the previous test), resulting in flakiness.
function navigateAndWait(url, callback) {
  const done =
      chrome.test.listenForever(chrome.tabs.onUpdated, function(_, info, tab) {
        if (debug) {
          console.log(`tabs.OnUpdated received in navigateAndWait: ${
              JSON.stringify(info)} ${JSON.stringify(tab)}`);
        }
        if (tab.id == tabId && info.status == 'complete') {
          if (callback)
            callback(tab);
          done();
        }
      });
  chrome.test.sendMessage(JSON.stringify({navigate: {tabId: tabId, url: url}}));
}

function deepCopy(obj) {
  if (obj === null) {
    return null;
  }
  if (typeof (obj) !== 'object') {
    return obj;
  }
  if (Array.isArray(obj)) {
    const tmpArray = new Array();
    for (let i = 0; i < obj.length; i++) {
      tmpArray.push(deepCopy(obj[i]));
    }
    return tmpArray;
  }

  const tmpObject = {};
  for (const p in obj) {
    tmpObject[p] = deepCopy(obj[p]);
  }
  return tmpObject;
}

// data: array of expected events, each one is a dictionary:
//     { label: '<unique identifier>',
//       event: '<webrequest event type>',
//       details: { <expected details of the webrequest event> },
//       retval: { <dictionary that the event handler shall return> } (optional)
//       retvalFunction: <function to run when the event occurs, this overrides
//                         any retval handling. The function takes
//                         (name, details, optional callback). The value it
//                         returns is returned out of the event
//                         handler> (optional)
//     }
// order: an array of sequences, e.g. [ ['a', 'b', 'c'], ['d', 'e'] ] means that
//     event with label 'a' needs to occur before event with label 'b'. The
//     relative order of 'a' and 'd' does not matter.
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
  tabAndFrameUrls = {};  // Maps '{tabId}-{frameId}' to the URL of the frame.
  frameIdMap = {'-1': -1, 0: 0};
  documentIdMap = [];
  removeListeners();
  resetDeclarativeRules();
  initListeners(filter || {urls: ['<all_urls>']}, extraInfoSpec || []);
  // Fill in default values.
  for (let i = 0; i < expectedEventData.length; ++i) {
    if (!('method' in expectedEventData[i].details)) {
      expectedEventData[i].details.method = 'GET';
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
      expectedEventData[i].details.type = 'main_frame';
    }
    if ('initiator' in expectedEventData[i].details &&
        expectedEventData[i].details.initiator == undefined) {
      delete expectedEventData[i].details.initiator;
    }
    if (expectedEventData[i].details.frameId >= 0) {
      if (!('documentLifecycle' in expectedEventData[i].details)) {
        expectedEventData[i].details.documentLifecycle = 'active';
      }
      if (!('frameType' in expectedEventData[i].details)) {
        expectedEventData[i].details.frameType = 'outermost_frame';
      }
    }
    if ('documentId' in expectedEventData[i].details &&
        expectedEventData[i].details.documentId == undefined) {
      delete expectedEventData[i].details.documentId;
    }
  }
}

function checkExpectations() {
  if (capturedEventData.length < expectedEventData.length) {
    return;
  }
  if (capturedEventData.length > expectedEventData.length) {
    chrome.test.fail(
        `Recorded too many events. ${JSON.stringify(capturedEventData)}`);
    return;
  }
  // We have ensured that capturedEventData contains exactly the same elements
  // as expectedEventData. Now we need to verify the ordering.
  // Step 1: build positions such that
  //     positions[<event-label>]=<position of this event in capturedEventData>
  let curPos = 0;
  const positions = {};
  capturedEventData.forEach(function(event) {
    chrome.test.assertTrue(event.hasOwnProperty('label'));
    positions[event.label] = curPos;
    curPos++;
  });
  // Step 2: check that elements arrived in correct order
  expectedEventOrder.forEach(function(order) {
    let previousLabel = undefined;
    order.forEach(function(label) {
      if (previousLabel === undefined) {
        previousLabel = label;
        return;
      }
      chrome.test.assertTrue(
          positions[previousLabel] < positions[label],
          `Event ${previousLabel} is supposed to arrive before ${label}.`);
      previousLabel = label;
    });
  });

  removeListeners();
  eventsCaptured();
}

// Simple check to see that we have a User-Agent header, and that it contains
// an expected value. This is a basic check that the request headers are valid.
function checkUserAgent(headers) {
  for (const i in headers) {
    if (headers[i].name.toLowerCase() == 'user-agent') {
      return headers[i].value.toLowerCase().indexOf('chrome') != -1;
    }
  }
  return false;
}

// Whether the request is missing a tabId and frameId and we're not expecting
// a request with the given details. If the method returns true, the event
// should be ignored.
function isUnexpectedDetachedRequest(name, details) {
  // This function is responsible for marking detached requests as unexpected.
  // Non-detached requests are not this function's concern.
  if (details.tabId !== -1 || details.frameId >= 0) {
    return false;
  }

  // Only return true if there is no matching expectation for the given details.
  return !expectedEventData.some(function(exp) {
    let didMatchTabAndFrameId =
        exp.details.tabId === -1 && exp.details.frameId === -1;

    // Accept non-matching tabId/frameId for ping/beacon requests because these
    // requests can continue after a frame is removed. And due to a bug, such
    // requests have a tabId/frameId of -1.
    // The test will fail anyway, but then with a helpful error (expectation
    // differs from actual events) instead of an obscure test timeout.
    // TODO(robwu): Remove this once https://crbug.com/40431900 gets fixed.
    didMatchTabAndFrameId = didMatchTabAndFrameId || details.type === 'ping';

    return name === exp.event && didMatchTabAndFrameId &&
        exp.details.method === details.method &&
        exp.details.url === details.url && exp.details.type === details.type;
  });
}

function captureEvent(name, details, callback) {
  // Ignore system-level requests like safebrowsing updates and favicon fetches
  // since they are unpredictable.
  if ((details.type == 'other' && !details.url.includes('dont-ignore-me')) ||
      isUnexpectedDetachedRequest(name, details) ||
      details.url.match(/\/favicon.ico$/) ||
      details.url.match(/https:\/\/dl.google.com/)) {
    return;
  }

  const originalDetails = deepCopy(details);

  // Check that the frameId can be used to reliably determine the URL of the
  // frame that caused requests.
  if (name == 'onBeforeRequest') {
    chrome.test.assertTrue(
        'frameId' in details && typeof details.frameId === 'number');
    chrome.test.assertTrue(
        'tabId' in details && typeof details.tabId === 'number');
    const key = `${details.tabId}-${details.frameId}`;
    if (details.type == 'main_frame' || details.type == 'sub_frame' ||
        details.type == 'webtransport') {
      tabAndFrameUrls[key] = details.url;
    }
    details.frameUrl = tabAndFrameUrls[key] || 'unknown frame URL';
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

  // Since the parentDocumentId & documentId is a unique random identifier it
  // is not useful to tests. Normalize it so that test cases can assert
  // against a fixed number.
  if ('parentDocumentId' in details) {
    if (documentIdMap[details.parentDocumentId] === undefined) {
      documentIdMap[details.parentDocumentId] =
          Object.keys(documentIdMap).length + 1;
    }
    details.parentDocumentId = documentIdMap[details.parentDocumentId];
  }
  if ('documentId' in details) {
    if (documentIdMap[details.documentId] === undefined) {
      documentIdMap[details.documentId] = Object.keys(documentIdMap).length + 1;
    }
    details.documentId = documentIdMap[details.documentId];
  }

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

  if (details?.requestBody?.raw) {
    for (const rawItem of details.requestBody.raw) {
      chrome.test.assertTrue(rawItem.bytes instanceof ArrayBuffer);
      // Stub out the bytes with an empty array buffer, since the expectations
      // don't hardcode real bytes.
      rawItem.bytes = new ArrayBuffer();
    }
  }

  // Check if the equivalent event is already captured, and issue a unique
  // |eventCount| to identify each.
  let eventCount = 0;
  capturedEventData.forEach(function(event) {
    if (deepEq(event.event, name) && deepEq(event.details, details)) {
      eventCount++;
      // update |details| for the next match.
      details.eventCount = eventCount;
    }
  });

  // find |details| in matchingExpectedEventData
  let matchingExpectedEvent = undefined;
  expectedEventData.forEach(function(exp) {
    if (deepEq(exp.event, name) && deepEq(exp.details, details)) {
      if (matchingExpectedEvent) {
        chrome.test.fail(
            `Duplicated expectation entry '${exp.label}' should be ` +
            `identified by |eventCount|: ${JSON.stringify(details)}`);
      } else {
        matchingExpectedEvent = exp;
      }
    }
  });
  if (!matchingExpectedEvent && !ignoreUnexpected) {
    console.log(
        `Expected events: ${JSON.stringify(expectedEventData, null, 2)}`);
    chrome.test.fail(`Received unexpected event '${name}':\n${
        JSON.stringify(details, null, 2)}`);
  }

  let retval;
  let retvalFunction;
  if (matchingExpectedEvent) {
    if (debug) {
      console.log(
          `Expected event received: ${name}: ${JSON.stringify(details)}`);
    }
    capturedEventData.push(
        {label: matchingExpectedEvent.label, event: name, details: details});

    // checkExpecations decrements the counter of pending events. We may only
    // call it if an expected event has occurred.
    checkExpectations();

    // Pull the extra per-event options out of the expected data. These let us
    // specify special return values per event.
    retval = matchingExpectedEvent.retval;
    retvalFunction = matchingExpectedEvent.retvalFunction;
  } else {
    if (debug) {
      console.log(
          `NOT Expected event received: ${name}: ${JSON.stringify(details)}`);
    }
    capturedUnexpectedData.push({event: name, details: details});
  }

  if (retvalFunction) {
    return retvalFunction(name, originalDetails, callback);
  }

  if (callback) {
    setTimeout(callback, 0, retval);
  } else {
    return retval;
  }
}

// Simple array intersection. We use this to filter extraInfoSpec so
// that only the allowed specs are sent to each listener.
function intersect(array1, array2) {
  return array1.filter(function(x) {
    return array2.indexOf(x) != -1;
  });
}

function initListeners(filter, extraInfoSpec) {
  const onBeforeRequest = function(details) {
    return captureEvent('onBeforeRequest', details);
  };
  listeners['onBeforeRequest'].push(onBeforeRequest);

  const onBeforeSendHeaders = function(details) {
    return captureEvent('onBeforeSendHeaders', details);
  };
  listeners['onBeforeSendHeaders'].push(onBeforeSendHeaders);

  const onSendHeaders = function(details) {
    return captureEvent('onSendHeaders', details);
  };
  listeners['onSendHeaders'].push(onSendHeaders);

  const onHeadersReceived = function(details) {
    return captureEvent('onHeadersReceived', details);
  };
  listeners['onHeadersReceived'].push(onHeadersReceived);

  const onAuthRequired = function(details, callback) {
    return captureEvent('onAuthRequired', details, callback);
  };
  listeners['onAuthRequired'].push(onAuthRequired);

  const onResponseStarted = function(details) {
    return captureEvent('onResponseStarted', details);
  };
  listeners['onResponseStarted'].push(onResponseStarted);

  const onBeforeRedirect = function(details) {
    return captureEvent('onBeforeRedirect', details);
  };
  listeners['onBeforeRedirect'].push(onBeforeRedirect);

  const onCompleted = function(details) {
    return captureEvent('onCompleted', details);
  };
  listeners['onCompleted'].push(onCompleted);

  const onErrorOccurred = function(details) {
    return captureEvent('onErrorOccurred', details);
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
      intersect(
          extraInfoSpec, ['blocking', 'responseHeaders', 'extraHeaders']));

  chrome.webRequest.onAuthRequired.addListener(
      onAuthRequired, filter, intersect(extraInfoSpec, [
        'asyncBlocking', 'blocking', 'responseHeaders', 'extraHeaders'
      ]));

  chrome.webRequest.onResponseStarted.addListener(
      onResponseStarted, filter,
      intersect(extraInfoSpec, ['responseHeaders', 'extraHeaders']));

  chrome.webRequest.onBeforeRedirect.addListener(
      onBeforeRedirect, filter,
      intersect(extraInfoSpec, ['responseHeaders', 'extraHeaders']));

  chrome.webRequest.onCompleted.addListener(
      onCompleted, filter,
      intersect(extraInfoSpec, ['responseHeaders', 'extraHeaders']));

  chrome.webRequest.onErrorOccurred.addListener(onErrorOccurred, filter);
}

function removeListeners() {
  function helper(eventName) {
    for (const i in listeners[eventName]) {
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
  const headerMap = {};
  for (let i = 0; i < headers.length; i++) {
    headerMap[headers[i].name.toLowerCase()] = headers[i].value;
  }

  for (let i = 0; i < requiredNames.length; i++) {
    chrome.test.assertTrue(
        !!headerMap[requiredNames[i]], `Missing header: ${requiredNames[i]}`);
  }
  for (let i = 0; i < disallowedNames.length; i++) {
    chrome.test.assertFalse(
        !!headerMap[disallowedNames[i]],
        `Header should not be present: ${disallowedNames[i]}`);
  }
}

function removeHeader(headers, name) {
  for (let i = 0; i < headers.length; i++) {
    if (headers[i].name.toLowerCase() == name) {
      headers.splice(i, 1);
      break;
    }
  }
}

function getPemEncodedFromDer(arrayBuffer) {
  // 1. Convert ArrayBuffer to Binary String.
  const uint8Arr = new Uint8Array(arrayBuffer);
  let binaryString = '';
  for (let i = 0; i < uint8Arr.byteLength; i++) {
    binaryString += String.fromCharCode(uint8Arr[i]);
  }

  // 2. Convert Binary String to Base64.
  const base64 = btoa(binaryString);

  // 3. Format as PEM (Wrap at 64 chars and add headers).
  const formattedBase64 = base64.match(/.{1,64}/g).join('\n');

  return '-----BEGIN CERTIFICATE-----\n' +
      `${formattedBase64}\n-----END CERTIFICATE-----\n`;
}
