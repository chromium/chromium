// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var util = {};
var embedder = {};
embedder.baseGuestURL = '';
embedder.emptyGuestURL = '';
embedder.windowOpenGuestURL = '';
embedder.noReferrerGuestURL = '';
embedder.redirectGuestURL = '';
embedder.redirectGuestURLDest = '';
embedder.closeSocketURL = '';
embedder.tests = {};

var request_to_comm_channel_1 = 'connect';
var request_to_comm_channel_2 = 'connect_request';
var response_from_comm_channel_1 = 'connected';
var response_from_comm_channel_2 = 'connected_response';

var GUEST_REDIRECT_FILE_NAME = 'guest_redirect.html';

embedder.setUp_ = function(config) {
  if (!config || !config.testServer) {
    return;
  }
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.echoURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/echo.html';
  embedder.emptyGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/empty_guest.html';
  embedder.windowOpenGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest.html';
  embedder.windowOpenNoopenerGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest_noopener.html';
  embedder.windowOpenGuestFromSameURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest_from_opener.html';
  embedder.noReferrerGuestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/guest_noreferrer.html';
  embedder.windowOpenMessageURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/window_open_message.html';
  embedder.detectUserAgentURL = embedder.baseGuestURL + '/detect-user-agent';
  embedder.redirectGuestURL = embedder.baseGuestURL + '/server-redirect';
  embedder.redirectGuestURLDest = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/' + GUEST_REDIRECT_FILE_NAME;
  embedder.closeSocketURL = embedder.baseGuestURL + '/close-socket';
  embedder.testImageBaseURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/';
  embedder.pluginURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/embed.html';
  embedder.mailtoTestURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/mailto.html';
  embedder.safeBrowsingDangerousURL = 'http://evil.com:' +
      config.testServer.port + '/title1.html';
};

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

var LOG = function(msg) {
  window.console.log(msg);
};

// Creates a <webview> tag in document.body and returns the reference to it.
// It also sets a dummy src. The dummy src is significant because this makes
// sure that the <object> shim is created (asynchronously at this point) for the
// <webview> tag. This makes the <webview> tag ready for add/removeEventListener
// calls.
util.createWebViewTagInDOM = function(partitionName) {
  var webview = document.createElement('webview');
  webview.style.width = '300px';
  webview.style.height = '200px';
  var urlDummy = 'data:text/html,<body>Initial dummy guest</body>';
  webview.setAttribute('src', urlDummy);
  webview.setAttribute('partition', partitionName);
  document.body.appendChild(webview);
  return webview;
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b, message) {
  if (a != b) {
    console.log(
        'assertion failed: ' + a + ' != ' + b +
        (message ? (': ' + message) : ''));
    embedder.test.fail();
  }
};

embedder.test.assertTrue = function(condition) {
  if (!condition) {
    console.log('assertion failed: true != ' + condition);
    embedder.test.fail();
  }
};

embedder.test.assertFalse = function(condition) {
  if (condition) {
    console.log('assertion failed: false != ' + condition);
    embedder.test.fail();
  }
};

// Promisify webview.executeScript.
function executeScriptP(webview, details) {
  return new Promise((resolve) => {
    webview.executeScript(details, resolve);
  });
}

// Promisify webview.getZoom.
function getZoomP(webview) {
  return new Promise((resolve) => {
    webview.getZoom((zoomFactor) => {
      resolve(zoomFactor);
    });
  });
}

// Promisify webview.setZoom.
function setZoomP(webview, zoomFactor) {
  return new Promise((resolve) => {
    webview.setZoom(zoomFactor, () => {
      resolve();
    });
  });
}

// Executes `fn` in the context of the `webview` with the given `args`. `fn`
// must be written in a way that it can be serialized as a string. So anything
// it references from this context must be passed explicitly via `args`.
// This can be used for cases where `webview.executeScript` is inadequate, such
// as testing APIs that are async. This is loosely based on RemoteContext's
// script execution from web-platform-tests.
async function evalInWebView(webview, fn, args) {
  // We have this handler run in the context of the webview where it will eval
  // the function and reply to the embedder with the result.
  let messageHandlerInWebview = async (event) => {
    try {
      let task = event.data;
      let result = await eval(task.fn).apply(null, task.args);
      event.source.postMessage({success: true, result: result}, event.origin);
    } catch (ex) {
      event.source.postMessage({success: false, result: ex}, event.origin);
    }
  };
  await executeScriptP(webview, {
    code: 'window.addEventListener(\'message\', ' +
        messageHandlerInWebview.toString() + ', {once: true});'
  });

  return new Promise((resolve, reject) => {
    window.addEventListener('message', (e) => {
      if (e.data.success) {
        resolve(e.data.result);
      } else {
        reject(e.data.result);
      }
    });

    let task = {fn: fn.toString(), args: args};
    webview.contentWindow.postMessage(task, '*');
  });
}

// Tests begin.

// This test verifies that the allowtransparency property is interpreted as true
// if it exists (regardless of its value), and can be removed by setting it to
// to anything false.
function testAllowTransparencyAttribute() {
  var webview = document.createElement('webview');
  webview.src = 'data:text/html,webview test';
  embedder.test.assertFalse(webview.hasAttribute('allowtransparency'));
  embedder.test.assertFalse(webview.allowtransparency);
  webview.allowtransparency = true;

  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(webview.hasAttribute('allowtransparency'));
    embedder.test.assertTrue(webview.allowtransparency);
    webview.allowtransparency = false;
    embedder.test.assertFalse(webview.hasAttribute('allowtransparency'));
    embedder.test.assertFalse(webview.allowtransparency);
    webview.allowtransparency = '';
    embedder.test.assertFalse(webview.hasAttribute('allowtransparency'));
    embedder.test.assertFalse(webview.allowtransparency);
    webview.allowtransparency = 'some string';
    embedder.test.assertTrue(webview.hasAttribute('allowtransparency'));
    embedder.test.assertTrue(webview.allowtransparency);
    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

// This test verifies that a lengthy page with autosize enabled will report
// the correct height in the sizechanged event.
function testAutosizeHeight() {
  var webview = document.createElement('webview');

  webview.autosize = true;
  webview.minwidth = 200;
  webview.maxwidth = 210;
  webview.minheight = 40;
  webview.maxheight = 200;

  var step = 1;
  var finalWidth = 200;
  var finalHeight = 50;
  webview.addEventListener('sizechanged', function(e) {
    embedder.test.assertTrue(e.newHeight >= webview.minheight);
    embedder.test.assertTrue(e.newHeight <= webview.maxheight);
    embedder.test.assertTrue(e.newWidth >= webview.minwidth);
    embedder.test.assertTrue(e.newWidth <= webview.maxwidth);
    if (step == 1)
      webview.maxheight = 50;

    // We are done once the size settles on the final width and height.
    if (e.newHeight == finalHeight && e.newWidth == finalWidth)
      embedder.test.succeed();
    ++step;
  });

  webview.src = 'data:text/html,' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>' +
                'a<br/>b<br/>c<br/>d<br/>e<br/>f<br/>';
  document.body.appendChild(webview);
}

// This test verifies that if a browser plugin is in autosize mode before
// navigation then the guest starts auto-sized.
function testAutosizeBeforeNavigation() {
  var webview = document.createElement('webview');

  webview.setAttribute('autosize', 'true');
  webview.setAttribute('minwidth', 200);
  webview.setAttribute('maxwidth', 210);
  webview.setAttribute('minheight', 100);
  webview.setAttribute('maxheight', 110);

  webview.addEventListener('sizechanged', function(e) {
    // The old size should be the default size of the webview, which at the time
    // of writing this comment is 300 x 300, but is not important to this test
    // and could change in the future, so it is not checked here.
    embedder.test.assertTrue(e.newWidth >= 200 && e.newWidth <= 210);
    embedder.test.assertTrue(e.newHeight >= 100 && e.newHeight <= 110);
    embedder.test.succeed();
  });

  webview.setAttribute('src', 'data:text/html,webview test sizechanged event');
  document.body.appendChild(webview);
}

// Makes sure 'sizechanged' event is fired only if autosize attribute is
// specified.
// After loading <webview> without autosize attribute and a size, say size1,
// we set autosize attribute and new min size with size2. We would get (only
// one) sizechanged event with size1 as old size and size2 as new size.
function testAutosizeAfterNavigation() {
  var webview = document.createElement('webview');

  var step = 1;
  var autosizeWidth = -1;
  var autosizeHeight = -1;
  var sizeChangeHandler = function(e) {
    switch (step) {
      case 1:
        // This would be triggered after we set autosize attribute.
        embedder.test.assertEq(50, e.oldWidth);
        embedder.test.assertEq(100, e.oldHeight);
        embedder.test.assertTrue(e.newWidth >= 60 && e.newWidth <= 70);
        embedder.test.assertTrue(e.newHeight >= 110 && e.newHeight <= 120);

        // Remove autosize attribute and expect webview to retain the same size.
        autosizeWidth = e.newWidth;
        autosizeHeight = e.newHeight;
        webview.removeAttribute('autosize');
        break;
      case 2:
        // Expect the autosized size.
        embedder.test.assertEq(autosizeWidth, e.newWidth);
        embedder.test.assertEq(autosizeHeight, e.newHeight);

        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }

    ++step;
  };

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.addEventListener('loadstop', function(e) {
    webview.setAttribute('autosize', true);
    webview.setAttribute('minwidth', 60);
    webview.setAttribute('maxwidth', 70);
    webview.setAttribute('minheight', 110);
    webview.setAttribute('maxheight', 120);
  });

  webview.style.width = '50px';
  webview.style.height = '100px';
  webview.setAttribute('src', 'data:text/html,webview test sizechanged event');
  document.body.appendChild(webview);
}

// This test verifies that autosize works when some of the parameters are unset.
function testAutosizeWithPartialAttributes() {
  window.console.log('testAutosizeWithPartialAttributes');
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    window.console.log('sizeChangeHandler, new: ' +
                       e.newWidth + ' X ' + e.newHeight);
    switch (step) {
      case 1:
        // Expect 300x200.
        embedder.test.assertEq(300, e.newWidth);
        embedder.test.assertEq(200, e.newHeight);

        // Change the min size to cause a relayout.
        webview.minwidth = 500;
        break;
      case 2:
        embedder.test.assertTrue(e.newWidth >= webview.minwidth);
        embedder.test.assertTrue(e.newWidth <= webview.maxwidth);

        // Tests when minwidth > maxwidth, minwidth = maxwidth.
        // i.e. minwidth is essentially 700.
        webview.minwidth = 800;
        break;
      case 3:
        // Expect 700X?
        embedder.test.assertEq(700, e.newWidth);
        embedder.test.assertTrue(e.newHeight >= 200);
        embedder.test.assertTrue(e.newHeight <= 600);

        embedder.test.succeed();
        break;
      default:
        window.console.log('Unexpected sizechanged event, step = ' + step);
        embedder.test.fail();
        break;
    }

    ++step;
  };

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.addEventListener('loadstop', function(e) {
    webview.minwidth = 300;
    webview.maxwidth = 700;
    webview.minheight = 200;
    webview.maxheight = 600;
    webview.autosize = true;
  });

  webview.style.width = '640px';
  webview.style.height = '480px';
  webview.setAttribute('src', 'data:text/html,webview check autosize');
  document.body.appendChild(webview);
}

// This test verifies that all autosize attributes can be removed
// without crashing the plugin, or throwing errors.
function testAutosizeRemoveAttributes() {
  var webview = document.createElement('webview');

  var step = 1;
  var sizeChangeHandler = function(e) {
    switch (step) {
      case 1:
        // This is the sizechanged event for autosize.

        // Remove attributes.
        webview.removeAttribute('minwidth');
        webview.removeAttribute('maxwidth');
        webview.removeAttribute('minheight');
        webview.removeAttribute('maxheight');
        webview.removeAttribute('autosize');

        // We'd get one more sizechanged event after we turn off
        // autosize.
        webview.style.width = '500px';
        webview.style.height = '500px';
        break;
      case 2:
        embedder.test.succeed();
        break;
    }

    ++step;
  };

  webview.addEventListener('loadstop', function(e) {
    webview.minwidth = 300;
    webview.maxwidth = 700;
    webview.minheight = 600;
    webview.maxheight = 400;
    webview.autosize = true;
  });

  webview.addEventListener('sizechanged', sizeChangeHandler);

  webview.style.width = '640px';
  webview.style.height = '480px';
  webview.setAttribute('src', 'data:text/html,webview check autosize');
  document.body.appendChild(webview);
}

function testAPIMethodExistence() {
  // See public-facing API functions in web_view_api_methods.js
  var WEB_VIEW_API_METHODS = [
    'addContentScripts',
    'back',
    'canGoBack',
    'canGoForward',
    'captureVisibleRegion',
    'clearData',
    'executeScript',
    'find',
    'forward',
    'getAudioState',
    'getProcessId',
    'getUserAgent',
    'getZoom',
    'getZoomMode',
    'go',
    'insertCSS',
    'isAudioMuted',
    'isSpatialNavigationEnabled',
    'isUserAgentOverridden',
    'loadDataWithBaseUrl',
    'print',
    'removeContentScripts',
    'reload',
    'setAudioMuted',
    'setSpatialNavigationEnabled',
    'setUserAgentOverride',
    'setZoom',
    'setZoomMode',
    'stop',
    'stopFinding',
    'terminate'
  ];

  var webview = document.createElement('webview');

  for (var methodName of WEB_VIEW_API_METHODS) {
    embedder.test.assertEq(
        'function', typeof webview[methodName],
        methodName + ' should be defined');
  }

  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    // Check contentWindow.
    embedder.test.assertEq('object', typeof webview.contentWindow);
    embedder.test.assertEq('function',
                         typeof webview.contentWindow.postMessage);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

function testCustomElementCallbacksInaccessible() {
  var CUSTOM_ELEMENT_CALLBACKS = [
    'connectedCallback',
    'disconnectedCallback',
    'attributeChangedCallback',
    'adoptedCallback'
  ];

  var webview = document.createElement('webview');
  for (var callbackName of CUSTOM_ELEMENT_CALLBACKS) {
    embedder.test.assertEq(
        'undefined', typeof webview[callbackName],
        callbackName + ' should not be accessible');
  }

  embedder.test.assertEq(
      'undefined', typeof webview.constructor['observedAttributes'],
      'observedAttributes should not be accessible');

  embedder.test.succeed();
}

// This test verifies that the loadstop event fires when loading a webview
// accessible resource from a partition that is privileged.
function testChromeExtensionURL() {
  var localResource = chrome.runtime.getURL('guest_with_inline_script.html');
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', localResource);
  document.body.appendChild(webview);
}

// This test verifies that the loadstop event fires when loading a webview
// accessible resource from a partition that is privileged if the src URL
// is not fully qualified.
function testChromeExtensionRelativePath() {
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'guest_with_inline_script.html');
  document.body.appendChild(webview);
}

// This test verifies that guests are blocked from navigating the webview to a
// data URL.
function testContentInitiatedNavigationToDataUrlBlocked() {
  var navUrl = "data:text/html,foo";
  var webview = document.createElement('webview');
  webview.addEventListener('consolemessage', function(e) {
    if (e.message.startsWith(
        'Not allowed to navigate top frame to data URL:')) {
      embedder.test.succeed();
    }
  });
  webview.addEventListener('loadstop', function(e) {
    if (webview.getAttribute('src') == navUrl) {
      embedder.test.fail();
    }
  });
  webview.setAttribute('src',
      'data:text/html,<script>window.location.href = "' + navUrl +
      '";</scr' + 'ipt>');
  document.body.appendChild(webview);
}

// Tests that a <webview> that starts with "display: none" style loads
// properly.
function testDisplayNoneWebviewLoad() {
  var webview = document.createElement('webview');
  var visible = false;
  webview.style.display = 'none';
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(visible);
    embedder.test.succeed();
  });
  // Set the .src while we are "display: none".
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);

  setTimeout(function() {
    visible = true;
    // This should trigger loadstop.
    webview.style.display = '';
  }, 0);
}

function testDisplayNoneWebviewRemoveChild() {
  var webview = document.createElement('webview');
  var visibleAndInDOM = false;
  webview.style.display = 'none';
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertTrue(visibleAndInDOM);
    embedder.test.succeed();
  });
  // Set the .src while we are "display: none".
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);

  setTimeout(function() {
    webview.parentNode.removeChild(webview);
    webview.style.display = '';
    visibleAndInDOM = true;
    // This should trigger loadstop.
    document.body.appendChild(webview);
  }, 0);
}

// Makes sure inline scripts works inside guest that was loaded from
// accessible_resources.
function testInlineScriptFromAccessibleResources() {
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.fail();
  });
  webview.addEventListener('consolemessage', function(e) {
    window.console.log('consolemessage: ' + e.message);
    if (e.message == 'guest_with_inline_script.html: Inline script ran') {
      embedder.test.succeed();
    }
  });
  webview.setAttribute('src', 'guest_with_inline_script.html');
  document.body.appendChild(webview);
}

// This tests verifies that webview fires a loadabort event instead of crashing
// the browser if we attempt to navigate to a chrome-extension: URL with an
// extension ID that does not exist.
function testInvalidChromeExtensionURL() {
  var invalidResource = 'chrome-extension://abc123/guest.html';
  var webview = document.createElement('webview');
  // foobar is a privileged partition according to the manifest file.
  webview.partition = 'foobar';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', invalidResource);
  document.body.appendChild(webview);
}

function testWebRequestAPIExistence() {
  var regularEventsToCheck = [
    // Declarative WebRequest API.
    'onMessage',
    // WebRequest API.
    'onBeforeRequest',
    'onBeforeSendHeaders',
    'onSendHeaders',
    'onHeadersReceived',
    'onAuthRequired',
    'onBeforeRedirect',
    'onResponseStarted',
    'onCompleted',
    'onErrorOccurred'
  ];
  var declarativeEventsToCheck = [
    // Declarative WebRequest API.
    'onRequest',
  ];
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function(e) {
    for (var i = 0; i < regularEventsToCheck.length; ++i) {
      var eventName = regularEventsToCheck[i];
      var event = webview.request[eventName];
      embedder.test.assertEq('object', typeof event);
      embedder.test.assertEq('function', typeof event.addListener);
    }

    // Ensure that the "onActionIgnored" event is not supported for webviews.
    embedder.test.assertFalse('onActionIgnored' in webview.request);

    for (var i = 0; i < declarativeEventsToCheck.length; ++i) {
      var eventName = declarativeEventsToCheck[i];
      var event = webview.request[eventName];
      embedder.test.assertEq('function', typeof event.addRules);
      embedder.test.assertEq('function', typeof event.getRules);
      embedder.test.assertEq('function', typeof event.removeRules);
    }

    // Try to overwrite webview.request, shall not succeed.
    webview.request = '123';
    embedder.test.assertTrue(typeof webview.request !== 'string');

    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,webview check api');
  document.body.appendChild(webview);
}

// Tests that calling addListener() succeeds on all WebRequest API events.
function testWebRequestAPIAddListener() {
  var webview = new WebView();

  [webview.request.onBeforeRequest,
   webview.request.onBeforeSendHeaders,
   webview.request.onSendHeaders,
   webview.request.onHeadersReceived,
   webview.request.onAuthRequired,
   webview.request.onBeforeRedirect,
   webview.request.onCompleted,
   webview.request.onErrorOccurred,
  ].forEach(function(event) {
    event.addListener(function(){}, {urls: ['<all_urls>']});
  });

  webview.onloadstop = function() { embedder.test.succeed(); };
  webview.src = 'about:blank';
  document.body.appendChild(webview);
}

// This test verifies that the loadstart, loadstop, and exit events fire as
// expected.
function testEventName() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);

  webview.addEventListener('loadstart', function(evt) {
    embedder.test.assertEq('loadstart', evt.type);
  });

  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq('loadstop', evt.type);
    webview.terminate();
  });

  webview.addEventListener('exit', function(evt) {
    embedder.test.assertEq('exit', evt.type);
    embedder.test.succeed();
  });

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

function testOnEventProperties() {
  var sequence = ['first', 'second', 'third', 'fourth'];
  var webview = document.createElement('webview');
  function createHandler(id) {
    return function(e) {
      embedder.test.assertEq(id, sequence.shift());
    };
  }

  webview.addEventListener('loadstart', createHandler('first'));
  webview.addEventListener('loadstart', createHandler('second'));
  webview.onloadstart = createHandler('third');
  webview.addEventListener('loadstart', createHandler('fourth'));
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(0, sequence.length);

    // Test that setting another 'onloadstart' handler replaces the previous
    // handler.
    sequence = ['first', 'second', 'fourth'];
    webview.onloadstart = function() {
      embedder.test.assertEq(0, sequence.length);
      embedder.test.succeed();
    };

    webview.setAttribute('src', 'data:text/html,next navigation');
  });

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// Tests that the 'loadprogress' event is triggered correctly.
function testLoadProgressEvent() {
  var webview = document.createElement('webview');
  var progress = 0;

  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(1, progress);
    embedder.test.succeed();
  });

  webview.addEventListener('loadprogress', function(evt) {
    progress = evt.progress;
  });

  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test registers two listeners on an event (loadcommit) and removes
// the <webview> tag when the first listener fires.
// Current expected behavior is that the second event listener will still
// fire without crashing.
function testDestroyOnEventListener() {
  var webview = document.createElement('webview');
  var url = 'data:text/html,<body>Destroy test</body>';

  var loadCommitCount = 0;
  function loadCommitCommon(e) {
    embedder.test.assertEq('loadcommit', e.type);
    if (url != e.url)
      return;
    ++loadCommitCount;
    if (loadCommitCount == 2) {
      // Pass in a timeout so that we can catch if any additional loadcommit
      // occurs.
      setTimeout(function() {
        embedder.test.succeed();
      }, 0);
    } else if (loadCommitCount > 2) {
      embedder.test.fail();
    }
  };

  // The test starts from here, by setting the src to |url|.
  webview.addEventListener('loadcommit', function(e) {
    window.console.log('loadcommit1');
    webview.parentNode.removeChild(webview);
    loadCommitCommon(e);
  });
  webview.addEventListener('loadcommit', function(e) {
    window.console.log('loadcommit2');
    loadCommitCommon(e);
  });
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// This test registers two event listeners on a same event (loadcommit).
// Each of the listener tries to change some properties on the event param,
// which should not be possible.
function testCannotMutateEventName() {
  var webview = document.createElement('webview');
  var url = 'data:text/html,<body>Two</body>';

  var loadCommitACalled = false;
  var loadCommitBCalled = false;

  var maybeFinishTest = function(e) {
    if (loadCommitACalled && loadCommitBCalled) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.succeed();
    }
  };

  var onLoadCommitA = function(e) {
    if (e.url == url) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertFalse(loadCommitACalled);
      loadCommitACalled = true;
      // Try mucking with properities inside |e|.
      e.type = 'modified';
      maybeFinishTest(e);
    }
  };
  var onLoadCommitB = function(e) {
    if (e.url == url) {
      embedder.test.assertEq('loadcommit', e.type);
      embedder.test.assertTrue(e.isTopLevel);
      embedder.test.assertFalse(loadCommitBCalled);
      loadCommitBCalled = true;
      // Try mucking with properities inside |e|.
      e.type = 'modified';
      maybeFinishTest(e);
    }
  };

  // The test starts from here, by setting the src to |url|. Event
  // listener registration works because we already have a (dummy) src set
  // on the <webview> tag.
  webview.addEventListener('loadcommit', onLoadCommitA);
  webview.addEventListener('loadcommit', onLoadCommitB);
  webview.setAttribute('src', url);
  document.body.appendChild(webview);
}

// This test verifies that the partion attribute cannot be changed after the src
// has been set.
function testPartitionChangeAfterNavigation() {
  var webview = document.createElement('webview');
  var partitionAttribute = arguments.callee.name;
  webview.setAttribute('partition', partitionAttribute);

  var loadstopHandler = function(e) {
    webview.partition = 'illegal';
    embedder.test.assertEq(partitionAttribute, webview.partition);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', loadstopHandler);

  document.body.appendChild(webview);
  webview.setAttribute('src', 'data:text/html,trigger navigation');
}

// This test verifies that removing partition attribute after navigation does
// not work, i.e. the partition remains the same.
function testPartitionRemovalAfterNavigationFails() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);

  var partition = 'testme';
  webview.setAttribute('partition', partition);

  var loadstopHandler = function(e) {
    window.console.log('webview.loadstop');
    // Removing after navigation should not change the partition.
    webview.removeAttribute('partition');
    embedder.test.assertEq('testme', webview.partition);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', loadstopHandler);

  webview.setAttribute('src', 'data:text/html,<html><body>guest</body></html>');
}

// This test verifies that a content script will be injected to the webview when
// the webview is navigated to a page that matches the URL pattern defined in
// the content sript.
function testAddContentScript() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts(
      [{name: 'myrule',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel.js']
        },
        run_at: 'document_start'}]);

  webview.addEventListener('loadstop', function() {
    var msg = [request_to_comm_channel_1];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  });

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == response_from_comm_channel_1) {
      console.log(
          'Step 2: A communication channel has been established with webview.');
      embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// Adds two content scripts with the same URL pattern to <webview> at the same
// time. This test verifies that both scripts are injected when the <webview>
// navigates to a URL that matches the URL pattern.
function testAddMultipleContentScripts() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts(myrule1 & myrule2)');
  webview.addContentScripts(
      [{name: 'myrule1',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel.js']
        },
        run_at: 'document_start'},
       {name: 'myrule2',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel_2.js']
        },
        run_at: 'document_start'}]);

  webview.addEventListener('loadstop', function() {
    var msg1 = [request_to_comm_channel_1];
    webview.contentWindow.postMessage(JSON.stringify(msg1), '*');
    var msg2 = [request_to_comm_channel_2];
    webview.contentWindow.postMessage(JSON.stringify(msg2), '*');
  });

  var response_1 = false;
  var response_2 = false;
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == response_from_comm_channel_1) {
      console.log(
          'Step 2: A communication channel has been established with webview.');
      response_1 = true;
      if (response_1 && response_2)
        embedder.test.succeed();
      return;
    } else if (data == response_from_comm_channel_2) {
      console.log(
          'Step 3: A communication channel has been established with webview.');
      response_2 = true;
      if (response_1 && response_2)
        embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// Adds a content script to <webview> and navigates. After seeing the script is
// injected, we add another content script with the same name to the <webview>.
// This test verifies that the second script will replace the first one and be
// injected after navigating the <webview>. Meanwhile, the <webview> shouldn't
// get any message from the first script anymore.
function testAddContentScriptWithSameNameShouldOverwriteTheExistingOne() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts(myrule1)');
  webview.addContentScripts(
      [{name: 'myrule1',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel.js']
        },
        run_at: 'document_start'}]);
  var connect_script_1 = true;
  var connect_script_2 = false;

  webview.addEventListener('loadstop', function() {
    if (connect_script_1) {
      var msg1 = [request_to_comm_channel_1];
      webview.contentWindow.postMessage(JSON.stringify(msg1), '*');
      connect_script_1 = false;
    }
    if (connect_script_2) {
      var msg2 = [request_to_comm_channel_2];
      webview.contentWindow.postMessage(JSON.stringify(msg2), '*');
      connect_script_2 = false;
    }
  });

  var should_get_response_from_script_1 = true;
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == response_from_comm_channel_1) {
      if (should_get_response_from_script_1) {
        console.log(
            'Step 2: A communication channel has been established with webview.'
            );
        webview.addContentScripts(
            [{name: 'myrule1',
              matches: ['http://*/extensions/*'],
              js: {
                files: ['inject_comm_channel_2.js']
              },
              run_at: 'document_start'}]);
        connect_script_2 = true;
        should_get_response_from_script_1 = false;
        webview.src = embedder.emptyGuestURL;
      } else {
        embedder.test.fail();
      }
      return;
    } else if (data == response_from_comm_channel_2) {
      console.log(
          'Step 3: Another communication channel has been established ' +
          'with webview.');
      setTimeout(function() {
        embedder.test.succeed();
      }, 0);
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// There are two <webview>s are added to the DOM, and we add a content script
// to one of them. This test verifies that the script won't be injected in
// the other <webview>.
function testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView() {
  var webview1 = document.createElement('webview');
  var webview2 = document.createElement('webview');

  console.log('Step 1: call <webview1>.addContentScripts.');
  webview1.addContentScripts(
      [{name: 'myrule',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel.js']
        },
        run_at: 'document_start'}]);

  webview2.addEventListener('loadstop', function() {
    console.log('Step 2: webview2 requests to build communication channel.');
    var msg = [request_to_comm_channel_1];
    webview2.contentWindow.postMessage(JSON.stringify(msg), '*');
    setTimeout(function() {
      embedder.test.succeed();
    }, 0);
  });

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == response_from_comm_channel_1) {
      embedder.test.fail();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview1.src = embedder.emptyGuestURL;
  webview2.src = embedder.emptyGuestURL;
  document.body.appendChild(webview1);
  document.body.appendChild(webview2);
}


// Adds a content script to <webview> and navigates to a URL that matches the
// URL pattern defined in the script. After the first navigation, we remove this
// script from the <webview> and navigates to the same URL. This test verifies
// taht the script is injected during the first navigation, but isn't injected
// after removing it.
function testAddAndRemoveContentScripts() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts(
      [{name: 'myrule',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel.js']
        },
        run_at: 'document_start'}]);

  var count = 0;
  webview.addEventListener('loadstop', function() {
    if (count == 0) {
      console.log('Step 2: post message to build connect.');
      var msg = [request_to_comm_channel_1];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      ++count;
    } else if (count == 1) {
      console.log(
          'Step 4: call <webview>.removeContentScripts and navigate.');
      webview.removeContentScripts();
      webview.src = embedder.emptyGuestURL;
      ++count;
    } else if (count == 2) {
      console.log('Step 5: post message to build connect again.');
      var msg = [request_to_comm_channel_1];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      setTimeout(function() {
        embedder.test.succeed();
      }, 0);
    }
  });

  var replyCount = 0;
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == response_from_comm_channel_1) {
      console.log(
          'Step 3: A communication channel has been established with webview.');
      if (replyCount == 0) {
        webview.setAttribute('src', 'about:blank');
        ++replyCount;
        return;
      } else if (replyCount == 1) {
        embedder.test.fail();
        return;
      }
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// This test verifies that the addContentScripts API works with the new window
// API.
function testAddContentScriptsWithNewWindowAPI() {
  var webview = document.createElement('webview');

  var newwebview;
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    newwebview = document.createElement('webview');

    console.log('Step 2: call newwebview.addContentScripts.');
    newwebview.addContentScripts(
        [{name: 'myrule',
          matches: ['http://*/extensions/*'],
          js: {
            files: ['inject_comm_channel.js']
          },
          run_at: 'document_start'}]);

    newwebview.addEventListener('loadstop', function(evt) {
      var msg = [request_to_comm_channel_1];
      console.log('Step 4: new webview postmessage to build communication ' +
          'channel.');
      newwebview.contentWindow.postMessage(JSON.stringify(msg), '*');
    });

    document.body.appendChild(newwebview);
    // attach the new window to the new <webview>.
    console.log('Step 3: attaches the new webview.');
    e.window.attach(newwebview);
  });

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == response_from_comm_channel_1 &&
        e.source == newwebview.contentWindow) {
      console.log('Step 5: a communication channel has been established ' +
          'with the new webview.');
      embedder.test.succeed();
      return;
    } else {
      embedder.test.fail();
      return;
    }
    console.log('unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  console.log('Step 1: navigates the webview to window open guest URL.');
  webview.setAttribute('src', embedder.windowOpenGuestFromSameURL);
  document.body.appendChild(webview);
}

// Adds a content script to <webview>. This test verifies that the script is
// injected after terminate and reload <webview>.
function testContentScriptIsInjectedAfterTerminateAndReloadWebView() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts(
      [{name: 'myrule',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['inject_comm_channel.js']
        },
        run_at: 'document_start'}]);

  var count = 0;
  webview.addEventListener('loadstop', function() {
    if (count == 0) {
      console.log('Step 2: call webview.terminate().');
      webview.terminate();
      ++count;
      return;
    } else if (count == 1) {
      console.log('Step 4: postMessage to build communication.');
      var msg = [request_to_comm_channel_1];
      webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      ++count;
    }
  });

  webview.addEventListener('exit', function() {
    console.log('Step 3: call webview.reload().');
    webview.reload();
  });

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data == response_from_comm_channel_1) {
      console.log(
          'Step 5: A communication channel has been established with webview.');
      embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// This test verifies the content script won't be removed when the guest is
// destroyed, i.e., removed <webview> from the DOM.
function testContentScriptExistsAsLongAsWebViewTagExists() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts(
      [{name: 'myrule',
        matches: ['http://*/extensions/*'],
        js: {
          files: ['simple_script.js']
        },
        run_at: 'document_end'}]);

  var count = 0;
  webview.addEventListener('loadstop', function() {
    if (count == 0) {
       console.log('Step 2: check the result of content script injected.');
      webview.executeScript({
        code: 'document.body.style.backgroundColor;'
      }, function(results) {
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq('red', results[0]);

        console.log('Step 3: remove webview from the DOM.');
        document.body.removeChild(webview);

        console.log('Step 4: add webview back to the DOM.');
        document.body.appendChild(webview);
        ++count;
      });
    } else if (count == 1) {
      webview.executeScript({
        code: 'document.body.style.backgroundColor;'
      }, function(results) {
        console.log('Step 5: check the result of content script injected' +
            ' again.');
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq('red', results[0]);
        embedder.test.succeed();
      });
    }
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

function testAddContentScriptWithCode() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts.');
  webview.addContentScripts(
      [{name: 'myrule',
        matches: ['http://*/extensions/*'],
        js: {
          code: 'document.body.style.backgroundColor = \'red\';'
        },
        run_at: 'document_end'}]);

  webview.addEventListener('loadstop', function() {
    console.log('Step 2: call webview.executeScript() to check result.')
    webview.executeScript({
      code: 'document.body.style.backgroundColor;'},
      function(results) {
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq('red', results[0]);
        embedder.test.succeed();
    });
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

function testAddMultipleContentScriptsWithCodeAndCheckGeneratedScriptUrl() {
  var webview = document.createElement('webview');

  console.log('Step 1: call <webview>.addContentScripts.');
  var getCode = function(id) {
    return 'var e = new Error();\n' +
           'var n = document.createElement("span");\n' +
           'n.id = "textnode' + id + '";\n' +
           'n.textContent = e.stack;\n' +
           'document.body.appendChild(n);\n';
  }
  var code0 = getCode('0');
  var code1 = getCode('1');
  webview.addContentScripts(
      [{name: 'myrule0',
        matches: ['http://*/extensions/*'],
        js: {code: code0},
        run_at: 'document_end'}]);
  webview.addContentScripts(
      [{name: 'myrule1',
        matches: ['http://*/extensions/*'],
        js: {code: code1},
        run_at: 'document_end'}]);

  webview.addEventListener('loadstop', function() {
    console.log('Step 2: call webview.executeScript() to check result.')
    webview.executeScript({
      code: '[document.getElementById("textnode0").textContent,' +
            'document.getElementById("textnode1").textContent];'},
      function(results) {
        embedder.test.assertEq(1, results.length);
        var contents = results[0];
        embedder.test.assertEq(2, contents.length);
        embedder.test.assertTrue(
            contents[0].indexOf('generated_script_file:') != -1);
        embedder.test.assertTrue(
            contents[1].indexOf('generated_script_file:') != -1);
        embedder.test.assertTrue(contents[0] != contents[1]);
        embedder.test.succeed();
    });
  });

  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

function testExecuteScriptFail() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);
  setTimeout(function() {
    webview.executeScript(
        {code:'document.body.style.backgroundColor = "red";'},
        function(results) {
          embedder.test.fail();
        });
    setTimeout(function() {
      embedder.test.succeed();
    }, 0);
  }, 0);
}

function testExecuteScript() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  webview.addEventListener('loadstop', function() {
    webview.executeScript(
      {code:'document.body.style.backgroundColor = "red";'},
      function(results) {
        embedder.test.assertEq(1, results.length);
        embedder.test.assertEq('red', results[0]);
        embedder.test.succeed();
      });
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that the call to executeScript will fail and return null
// if the webview has been navigated between the time the call was made and the
// time it arrives in the guest process.
function testExecuteScriptIsAbortedWhenWebViewSourceIsChanged() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function onLoadStop(e) {
    window.console.log('2. Inject script to trigger a guest-initiated ' +
        'navigation.');
    var navUrl = 'data:text/html,trigger nav';
    webview.executeScript({
      code: 'window.location.href = "' + navUrl + '";'
    });

    window.console.log('3. Listening for the load that will be started as a ' +
        'result of 2.');
    webview.addEventListener('loadstart', function onLoadStart(e) {
      embedder.test.assertEq('about:blank', webview.src);
      window.console.log('4. Attempting to inject script into about:blank. ' +
          'This is expected to fail.');
      webview.executeScript(
        { code: 'document.body.style.backgroundColor = "red";' },
        function(results) {
          window.console.log(
              '5. Verify that executeScript has, indeed, failed.');
          embedder.test.assertEq(null, results);
          embedder.test.assertEq(navUrl, webview.src);
          embedder.test.succeed();
        }
      );
      webview.removeEventListener('loadstart', onLoadStart);
    });
    webview.removeEventListener('loadstop', onLoadStop);
  });

  window.console.log('1. Performing initial navigation.');
  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);
}

// This test verifies that, in the case where the WebView is set up with a
// 'loadabort' handler that calls executeScript() with a script that sets
// the WebView's 'src' to an invalid URL, and the caller then calls
// executeScript() on that WebView with that same script (that sets 'src'
// to an invalid URL), that loadabort will get called in both cases (and
// that the browser does not crash during the second call to executeScript()).
function testExecuteScriptIsAbortedWhenWebViewSourceIsInvalid() {
  var webview = document.createElement('webview');
  var abortCount = 0;

  webview.addEventListener('loadstop', loadDone);
  webview.addEventListener('loadabort', loadAbort);
  webview.addEventListener('exit', function(e) {
    // We should not crash.
    embedder.test.fail();
  });

  function loadDone() {
    window.console.log(
        '2. WebView loaded \'about:blank\'.  Now call \'executeScript()\'');
    webview.executeScript( {code: '/* no op */'}, webviewStop);
  }

  function webviewStop() {
    window.console.log(
        '3. Executing the script.  Set webview.src to ' +
        '\'http:\' (which is invalid and should cause an abort)');
    webview.src = 'http:';
  }

  function loadAbort() {
    abortCount++;
    if (abortCount == 1) {
      window.console.log(
          '4. In \'loadabort\' handler.  Execute the script again, ' +
          'which should cause the \'loadabort\' handler to be called again ' +
          '(the browser should NOT crash)');
      webview.executeScript( {code: '/* no op */'}, webviewStop);
    } else {
      window.console.log('5. In \'loadabort\' handler for 2nd time. Success!!');
      embedder.test.succeed();
    }
  }

  window.console.log('1. Set webview.src to \'about:blank\'');
  webview.src = 'about:blank';
  document.body.appendChild(webview);
}

// This test calls terminate() on guest after it has already been
// terminated. This makes sure we ignore the call gracefully.
function testTerminateAfterExit() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var loadstopSucceedsTest = false;
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq('loadstop', evt.type);
    if (loadstopSucceedsTest) {
      embedder.test.succeed();
      return;
    }

    webview.terminate();
  });

  webview.addEventListener('exit', function(evt) {
    embedder.test.assertEq('exit', evt.type);
    // Call terminate again.
    webview.terminate();
    // Load another page. The test would pass when loadstop is called on
    // this second page. This would hopefully catch if call to
    // webview.terminate() caused a browser crash.
    setTimeout(function() {
      loadstopSucceedsTest = true;
      webview.setAttribute('src', 'data:text/html,test second page');
    }, 0);
  });

  webview.setAttribute('src', 'data:text/html,test terminate() crash.');
  document.body.appendChild(webview);
}

// This test verifies that multiple consecutive changes to the <webview> src
// attribute will cause a navigation.
function testNavOnConsecutiveSrcAttributeChanges() {
  var testPage1 = 'data:text/html,test page 1';
  var testPage2 = 'data:text/html,test page 2';
  var testPage3 = 'data:text/html,test page 3';
  var webview = new WebView();
  webview.partition = arguments.callee.name;
  var loadCommitCount = 0;
  webview.addEventListener('loadcommit', function(e) {
    if (e.url == testPage3) {
      embedder.test.succeed();
    }
    loadCommitCount++;
    if (loadCommitCount > 3) {
      embedder.test.fail();
    }
  });
  document.body.appendChild(webview);
  webview.src = testPage1;
  webview.src = testPage2;
  webview.src = testPage3;
}

function testNestedCrossOriginSubframes() {
  var webview = document.createElement('webview');
  var nestedFrameURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/parent_frame.html';
  webview.onconsolemessage = function(e) {
    window.console.log('guest.consolemessage ' + e.message);
  };
  webview.onloadstop = function() {
    // Only consider the first load stop, not the following one due to the
    // iframe navigation.
    webview.onloadstop = undefined;

    window.onmessage = function(e) {
      if (e.data == 'frames-loaded') {
        embedder.test.succeed();
      }
    };

    // Ask the <webview> to load nested frames. It will reply via postMessage
    // once frames have finished loading.
    webview.contentWindow.postMessage('load-frames', '*');
  };
  webview.onloadabort = embedder.test.fail;

  webview.src = nestedFrameURL;
  document.body.appendChild(webview);
}

function testNestedSubframes() {
  var webview = document.createElement('webview');
  webview.partition = 'foobar';
  var nestedFrameURL = 'parent_frame.html';
  webview.onconsolemessage = function(e) {
    window.console.log('guest.consolemessage ' + e.message);
  };
  webview.onloadstop = function() {
    // Only consider the first load stop, not the following one due to the
    // iframe navigation.
    webview.onloadstop = undefined;

    window.onmessage = function(e) {
      if (e.data == 'frames-loaded') {
        embedder.test.succeed();
      }
    };

    // Ask the <webview> to load nested frames. It will reply via postMessage
    // once frames have finished loading.
    webview.contentWindow.postMessage('load-frames', '*');
  };
  webview.onloadabort = embedder.test.fail;

  webview.src = nestedFrameURL;
  document.body.appendChild(webview);
}

// This test verifies that we can set the <webview> src multiple times and the
// changes will cause a navigation.
function testNavOnSrcAttributeChange() {
  var testPage1 = 'data:text/html,test page 1';
  var testPage2 = 'data:text/html,test page 2';
  var testPage3 = 'data:text/html,test page 3';
  var tests = [testPage1, testPage2, testPage3];
  var webview = new WebView();
  webview.partition = arguments.callee.name;
  var loadCommitCount = 0;
  webview.addEventListener('loadcommit', function(evt) {
    var success = tests.indexOf(evt.url) > -1;
    embedder.test.assertTrue(success);
    ++loadCommitCount;
    if (loadCommitCount == tests.length) {
      embedder.test.succeed();
    } else if (loadCommitCount > tests.length) {
      embedder.test.fail();
    } else {
      webview.src = tests[loadCommitCount];
    }
  });
  webview.src = tests[0];
  document.body.appendChild(webview);
}

// This test verifies that assigning the src attribute the same value it had
// prior to a crash spawns off a new guest process.
function testAssignSrcAfterCrash() {
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var terminated = false;
  webview.addEventListener('loadstop', function(evt) {
    if (!terminated) {
      webview.terminate();
      return;
    }
    // The guest has recovered after being terminated.
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(evt) {
    terminated = true;
    webview.setAttribute('src', 'data:text/html,test page');
  });
  webview.setAttribute('src', 'data:text/html,test page');
  document.body.appendChild(webview);
}

// This test verifies that <webview> reloads the page if the src attribute is
// assigned the same value.
function testReassignSrcAttribute() {
  var dataUrl = 'data:text/html,test page';
  var webview = new WebView();
  webview.partition = arguments.callee.name;

  var loadStopCount = 0;
  webview.addEventListener('loadstop', function(evt) {
    embedder.test.assertEq(dataUrl, webview.getAttribute('src'));
    ++loadStopCount;
    console.log('[' + loadStopCount + '] loadstop called');
    if (loadStopCount == 3) {
      embedder.test.succeed();
    } else if (loadStopCount > 3) {
      embedder.test.fail();
    } else {
      webview.src = dataUrl;
    }
  });
  webview.src = dataUrl;
  document.body.appendChild(webview);
}

// This test verifies that <webview> restores the src attribute if it is
// removed after navigation.
function testRemoveSrcAttribute() {
  var dataUrl = 'data:text/html,test page';
  var webview = document.createElement('webview');
  webview.setAttribute('partition', arguments.callee.name);
  var terminated = false;
  webview.addEventListener('loadstop', function(evt) {
    webview.removeAttribute('src');
    setTimeout(function() {
      embedder.test.assertEq(dataUrl, webview.getAttribute('src'));
      embedder.test.succeed();
    }, 0);
  });
  webview.setAttribute('src', dataUrl);
  document.body.appendChild(webview);
}

function testPluginLoadPermission() {
  var pluginIdentifier = 'unknown platform';
  if (navigator.platform.match(/linux/i))
    pluginIdentifier = 'libppapi_tests.so';
  else if (navigator.platform.match(/win32/i))
    pluginIdentifier = 'ppapi_tests.dll';
  else if (navigator.platform.match(/win64/i))
    pluginIdentifier = 'ppapi_tests.dll';
  else if (navigator.platform.match(/mac/i))
    pluginIdentifier = 'ppapi_tests.plugin';

  var webview = document.createElement('webview');
  webview.addEventListener('permissionrequest', function(e) {
    e.preventDefault();
    embedder.test.assertEq('loadplugin', e.permission);
    embedder.test.assertEq(pluginIdentifier, e.name);
    embedder.test.assertEq(pluginIdentifier, e.identifier);
    embedder.test.assertEq('function', typeof e.request.allow);
    embedder.test.assertEq('function', typeof e.request.deny);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,<body>' +
                              '<embed type="application/x-ppapi-tests">' +
                              '</embed></body>');
  document.body.appendChild(webview);
}

// This test verifies that new window attachment functions as expected.
function testNewWindow() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    newwebview.addEventListener('loadstop', function(evt) {
      // If the new window finishes loading, the test is successful.
      embedder.test.succeed();
    });
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    e.window.attach(newwebview);
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

// This test verifies "first-call-wins" semantics. That is, the first call
// to perform an action on the new window takes the action and all
// subsequent calls throw an exception.
function testNewWindowTwoListeners() {
  var webview = document.createElement('webview');
  var error = false;
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    document.body.appendChild(newwebview);
    try {
      e.window.attach(newwebview);
    } catch (err) {
      embedder.test.fail();
    }
  });
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    try {
      e.window.discard();
    } catch (err) {
      embedder.test.succeed();
    }
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

// This test verifies that the attach can be called inline without
// preventing default.
function testNewWindowNoPreventDefault() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    var newwebview = document.createElement('webview');
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    try {
      e.window.attach(newwebview);
      embedder.test.succeed();
    } catch (err) {
      embedder.test.fail();
    }
  });
  webview.setAttribute('src', embedder.windowOpenGuestURL);
  document.body.appendChild(webview);
}

function testNewWindowNoReferrerLink() {
  var webview = document.createElement('webview');
  webview.addEventListener('newwindow', function(e) {
    e.preventDefault();
    var newwebview = document.createElement('webview');
    newwebview.addEventListener('loadstop', function(evt) {
      // If the new window finishes loading, the test is successful.
      embedder.test.succeed();
    });
    document.body.appendChild(newwebview);
    // Attach the new window to the new <webview>.
    e.window.attach(newwebview);
  });
  webview.setAttribute('src', embedder.noReferrerGuestURL);
  document.body.appendChild(webview);
}

// Test that a webview guest can attach to a webview element with an existing
// guest.
function testNewWindowAttachToExisting() {
  let openerWebview = document.createElement('webview');
  openerWebview.src = embedder.windowOpenGuestURL;
  let otherWebview = document.createElement('webview');
  otherWebview.src = embedder.emptyGuestURL;

  openerWebview.addEventListener('newwindow', function(e) {
    e.preventDefault();

    otherWebview.addEventListener('loadstop', () => {
      embedder.test.succeed();
    }, { once: true });

    // Attach the new window to the existing webview.
    e.window.attach(otherWebview);
  }, { once: true });

  otherWebview.addEventListener('loadstop', () => {
    document.body.appendChild(openerWebview);
  }, { once: true });

  document.body.appendChild(otherWebview);
}

// This test verifies that the load event fires when the a new page is
// loaded.
// TODO(fsamuel): Add a test to verify that subframe loads within a guest
// do not fire the 'contentload' event.
function testContentLoadEvent() {
  var webview = document.createElement('webview');
  webview.addEventListener('contentload', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that the load event fires when the a new page is
// loaded even if the <webview> is set to display:none.
function testContentLoadEventWithDisplayNone() {
  var webview = document.createElement('webview');
  webview.style.display = 'none';
  webview.addEventListener('contentload', function(e) {
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  document.body.appendChild(webview);
}

// This test verifies that the WebRequest API onBeforeRequest event fires on
// webview.
function testWebRequestAPI() {
  var webview = new WebView();
  webview.request.onBeforeRequest.addListener(function(e) {
    embedder.test.succeed();
  }, { urls: ['<all_urls>']}) ;
  webview.src = embedder.windowOpenGuestURL;
  document.body.appendChild(webview);
}

// Like above, but ensures that a webview doesn't get events for other webviews.
function testWebRequestAPIOnlyForInstance() {
  var tempWebview = new WebView();
  tempWebview.request.onBeforeRequest.addListener(function(e) {
    embedder.test.fail();
  }, { urls: ['<all_urls>']}) ;
  testWebRequestAPI();
}

// This test verifies that the WebRequest API onBeforeSendHeaders event fires on
// webview and supports headers. This tests verifies that we can modify HTTP
// headers via the WebRequest API and those modified headers will be sent to the
// HTTP server.
function testWebRequestAPIWithHeaders() {
  var webview = new WebView();
  var requestFilter = {
    urls: ['<all_urls>']
  };
  var extraInfoSpec = ['requestHeaders', 'blocking'];
  webview.request.onBeforeSendHeaders.addListener(function(details) {
    var headers = details.requestHeaders;
    for( var i = 0, l = headers.length; i < l; ++i ) {
      if (headers[i].name.toLowerCase() == 'user-agent') {
        headers[i].value = 'foobar';
        break;
      }
    }
    var blockingResponse = {};
    blockingResponse.requestHeaders = headers;
    return blockingResponse;
  }, requestFilter, extraInfoSpec);

  var loadstartCalled = false;
  webview.addEventListener('loadstart', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.detectUserAgentURL, e.url);
    loadstartCalled = true;
  });

  webview.addEventListener('loadredirect', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.detectUserAgentURL,
        e.oldUrl.replace('127.0.0.1', 'localhost'));
    embedder.test.assertEq(embedder.redirectGuestURLDest,
        e.newUrl.replace('127.0.0.1', 'localhost'));
    if (loadstartCalled) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  webview.src = embedder.detectUserAgentURL;
  document.body.appendChild(webview);
}

function testWebRequestAPIErrorOccurred() {
  var webview = new WebView();

  webview.request.onErrorOccurred.addListener(function(details) {
    embedder.test.succeed();
  }, {urls: ['<all_urls>']});
  webview.request.onBeforeRequest.addListener(function(e) {
    return {cancel: true};
  }, {urls: ['<all_urls>']}, ['blocking']) ;

  webview.src = 'http://nonexistent.com';
  document.body.appendChild(webview);
}

// This test verifies that the basic use cases of the declarative WebRequest API
// work as expected. This test demonstrates that rules can be added prior to
// navigation and attachment.
// 1. It adds a rule to block URLs that contain guest.
// 2. It attempts to navigate to a guest.html page.
// 3. It detects the appropriate loadabort message.
// 4. It removes the rule blocking the page and reloads.
// 5. The page loads successfully.
function testDeclarativeWebRequestAPI() {
  var step = 1;
  var webview = new WebView();
  var rule = {
    conditions: [
      new chrome.webViewRequest.RequestMatcher(
        {
          url: { urlContains: 'guest' }
        }
      )
    ],
    actions: [
      new chrome.webViewRequest.CancelRequest()
    ]
  };
  webview.request.onRequest.addRules([rule]);
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(1, step);
    embedder.test.assertEq('ERR_BLOCKED_BY_CLIENT', e.reason);
    step = 2;
    webview.request.onRequest.removeRules();
    webview.reload();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq(2, step);
    embedder.test.succeed();
  });
  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

function testDeclarativeWebRequestAPISendMessage() {
  var webview = new WebView();
  window.console.log(embedder.emptyGuestURL);
  var rule = {
    conditions: [
      new chrome.webViewRequest.RequestMatcher(
        {
          url: { urlContains: 'guest' }
        }
      )
    ],
    actions: [
      new chrome.webViewRequest.SendMessageToExtension({ message: 'bleep' })
    ]
  };
  webview.request.onRequest.addRules([rule]);
  webview.request.onMessage.addListener(function(e) {
    embedder.test.assertEq('bleep', e.message);
    embedder.test.succeed();
  });
  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

function testDeclarativeWebRequestAPISendMessageSecondWebView() {
  var tempWebview = new WebView();
  testDeclarativeWebRequestAPISendMessage();
}

// This test verifies that setting a <webview>'s style.display = 'block' does
// not throw and attach error.
function testDisplayBlock() {
  var webview = new WebView();
  webview.onloadstop = function(e) {
    LOG('webview.onloadstop');
    window.console.error = function() {
      // If we see an error, that means attach failed.
      embedder.test.fail();
    };
    webview.style.display = 'block';
    embedder.test.assertTrue(webview.getProcessId() > 0);

    webview.onloadstop = function(e) {
      LOG('Second webview.onloadstop');
      embedder.test.succeed();
    };
    webview.src = 'data:text/html,<body>Second load</body>';
  }
  webview.src = 'about:blank';
  document.body.appendChild(webview);
}

// This test verifies that the WebRequest API onBeforeRequest event fires on
// clients*.google.com URLs.
function testWebRequestAPIGoogleProperty() {
  var webview = new WebView();
  webview.request.onBeforeRequest.addListener(function(e) {
    embedder.test.succeed();
    return {cancel: true};
  }, { urls: ['<all_urls>']}, ['blocking']) ;
  webview.src = 'http://clients6.google.com';
  document.body.appendChild(webview);
}

// This test verifies that the WebRequest event listener for onBeforeRequest
// survives reparenting of the <webview>.
function testWebRequestListenerSurvivesReparenting() {
  var webview = new WebView();
  var count = 0;
  webview.request.onBeforeRequest.addListener(function(e) {
    if (++count == 2) {
      embedder.test.succeed();
    }
  }, { urls: ['<all_urls>']});
  var onLoadStop =  function(e) {
    webview.removeEventListener('loadstop', onLoadStop);
    webview.parentNode.removeChild(webview);
    var container = document.getElementById('object-container');
    if (!container) {
      embedder.test.fail('Container for object not found.');
      return;
    }
    container.appendChild(webview);
  };
  webview.addEventListener('loadstop', onLoadStop);
  webview.src = embedder.emptyGuestURL;
  document.body.appendChild(webview);
}

// This test verifies that getProcessId is defined and returns a non-zero
// value corresponding to the processId of the guest process.
function testGetProcessId() {
  var webview = document.createElement('webview');
  webview.setAttribute('src', 'data:text/html,trigger navigation');
  var firstLoad = function() {
    webview.removeEventListener('loadstop', firstLoad);
    embedder.test.assertTrue(webview.getProcessId() > 0);
    embedder.test.succeed();
  };
  webview.addEventListener('loadstop', firstLoad);
  document.body.appendChild(webview);
}

function testHiddenBeforeNavigation() {
  var webview = document.createElement('webview');
  webview.style.visibility = 'hidden';

  var postMessageHandler = function(e) {
    var data = JSON.parse(e.data);
    window.removeEventListener('message', postMessageHandler);
    if (data[0] == 'visibilityState-response') {
      embedder.test.assertEq('hidden', data[1]);
      embedder.test.succeed();
    } else {
      LOG('Unexpected message: ' + data);
      embedder.test.fail();
    }
  };

  webview.addEventListener('loadstop', function(e) {
    LOG('webview.loadstop');
    window.addEventListener('message', postMessageHandler);
    webview.addEventListener('consolemessage', function(e) {
      LOG('g: ' + e.message);
    });

    webview.executeScript(
      {file: 'inject_hidden_test.js'},
      function(results) {
        if (!results || !results.length) {
          LOG('Failed to inject script: inject_hidden_test.js');
          embedder.test.fail();
          return;
        }

        LOG('script injection success');
        webview.contentWindow.postMessage(
            JSON.stringify(['visibilityState-request']), '*');
      });
  });

  webview.setAttribute('src', 'data:text/html,<html><body></body></html>');
  document.body.appendChild(webview);
}

// This test verifies that the loadstart event fires at the beginning of a load
// and the loadredirect event fires when a redirect occurs.
function testLoadStartLoadRedirect() {
  var webview = document.createElement('webview');
  var loadstartCalled = false;
  webview.setAttribute('src', embedder.redirectGuestURL);
  webview.addEventListener('loadstart', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.redirectGuestURL, e.url);
    loadstartCalled = true;
  });
  webview.addEventListener('loadredirect', function(e) {
    embedder.test.assertTrue(e.isTopLevel);
    embedder.test.assertEq(embedder.redirectGuestURL,
        e.oldUrl.replace('127.0.0.1', 'localhost'));
    embedder.test.assertEq(embedder.redirectGuestURLDest,
        e.newUrl.replace('127.0.0.1', 'localhost'));
    if (loadstartCalled) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires when loading a webview
// accessible resource from a partition that is not privileged.
function testLoadAbortChromeExtensionURLWrongPartition() {
  var localResource = chrome.runtime.getURL('guest.html');
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-20, e.code);
    embedder.test.assertEq('ERR_BLOCKED_BY_CLIENT', e.reason);
    embedder.test.succeed();
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.fail();
  });
  webview.setAttribute('src', localResource);
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires as expected and with the
// appropriate fields when an empty response is returned.
function testLoadAbortEmptyResponse() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-324, e.code);
    embedder.test.assertEq('ERR_EMPTY_RESPONSE', e.reason);
    embedder.test.succeed();
  });
  webview.setAttribute('src', embedder.closeSocketURL);
  document.body.appendChild(webview);
}

// This test verifies that the loadabort event fires as expected when an illegal
// chrome URL is provided.
function testLoadAbortIllegalChromeURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-301, e.code);
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
  });
  webview.addEventListener('loadstop', function(e)  {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.src = 'chrome://newtab';
  document.body.appendChild(webview);
}

function testLoadAbortIllegalFileURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-301, e.code);
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.src = 'file://foo';
  document.body.appendChild(webview);
}

function testLoadAbortIllegalJavaScriptURL() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-301, e.code);
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.setAttribute('src', 'javascript:void(document.bgColor="#0000FF")');
  document.body.appendChild(webview);
}

// Verifies that navigating to invalid URL (e.g. 'http:') doesn't cause a crash.
function testLoadAbortInvalidNavigation() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-300, e.code);
    embedder.test.assertEq('ERR_INVALID_URL', e.reason);
    embedder.test.assertEq('', e.url);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(e) {
    // We should not crash.
    embedder.test.fail();
  });
  webview.src = 'http:';
  document.body.appendChild(webview);
}

// Verifies that navigation to a URL that is valid but not web-safe or
// pseudo-scheme fires loadabort and doesn't cause a crash.
function testLoadAbortNonWebSafeScheme() {
  var webview = document.createElement('webview');
  var chromeUntrustedURL = 'chrome-untrusted://abc123/';
  webview.addEventListener('loadabort', function(e) {
    embedder.test.assertEq(-301, e.code);
    embedder.test.assertEq('ERR_DISALLOWED_URL_SCHEME', e.reason);
    embedder.test.assertEq(chromeUntrustedURL, e.url);
  });
  webview.addEventListener('loadstop', function(e) {
    embedder.test.assertEq('about:blank', webview.src);
    embedder.test.succeed();
  });
  webview.addEventListener('exit', function(e) {
    // We should not crash.
    embedder.test.fail();
  });
  webview.src = chromeUntrustedURL;
  document.body.appendChild(webview);
}

// Test that Safe Browsing is active inside webviews and that the embedder is
// notified of blocked loads. Furthermore, we ensure that the embedder itself
// is not disrupted by Safe Browsing for something that happened inside the
// webview.
function testLoadAbortSafeBrowsing() {
  let webview = document.createElement('webview');
  webview.addEventListener('loadabort', (e) => {
    embedder.test.assertEq(-20, e.code);
    embedder.test.assertEq('ERR_BLOCKED_BY_CLIENT', e.reason);

    // Safe Browsing prevented the load in the webview, but we also want to
    // ensure that Safe Browsing doesn't interfere with the webview's
    // embedder. So we'll wait for something safe to load in the webview to
    // confirm that it still works and the embedder doesn't get replaced by an
    // interstitial in the meantime.
    webview.src = embedder.emptyGuestURL;
  });
  webview.addEventListener('loadcommit', (e) => {
    if (e.url == embedder.safeBrowsingDangerousURL) {
      console.log('Committed dangerous URL in webview');
      embedder.test.fail();
    } else if (e.url == embedder.emptyGuestURL) {
      embedder.test.succeed();
    }
  });
  webview.src = embedder.safeBrowsingDangerousURL;
  document.body.appendChild(webview);
}

// This test verifies that the reload method on webview functions as expected.
function testReload() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  var loadCommitCount = 0;
  webview.addEventListener('loadstop', function(e) {
    if (loadCommitCount < 2) {
      webview.reload();
    } else if (loadCommitCount == 2) {
      embedder.test.succeed();
    } else {
      embedder.test.fail();
    }
  });
  webview.addEventListener('loadcommit', function(e) {
    embedder.test.assertEq(triggerNavUrl, e.url);
    embedder.test.assertTrue(e.isTopLevel);
    loadCommitCount++;
  });

  webview.setAttribute('src', triggerNavUrl);
  document.body.appendChild(webview);
}

// This test verifies that the reload method on webview functions as expected.
function testReloadAfterTerminate() {
  var triggerNavUrl = 'data:text/html,trigger navigation';
  var webview = document.createElement('webview');

  var step = 1;
  webview.addEventListener('loadstop', function(e) {
    switch (step) {
      case 1:
        webview.terminate();
        break;
      case 2:
        setTimeout(function() { embedder.test.succeed(); }, 0);
        break;
      default:
        window.console.log('Unexpected loadstop event, step = ' + step);
        embedder.test.fail();
        break;
    }
    ++step;
  });

  webview.addEventListener('exit', function(e) {
    // Trigger a focus state change of the guest to test for
    // http://crbug.com/413874.
    webview.blur();
    webview.focus();
    setTimeout(function() { webview.reload(); }, 0);
  });

  webview.src = triggerNavUrl;
  document.body.appendChild(webview);
}

// This test verifies that a <webview> is torn down gracefully when removed from
// the DOM on exit.

window.removeWebviewOnExitDoCrash = null;

function testRemoveWebviewOnExit() {
  var webview = document.createElement('webview');

  webview.addEventListener('loadstop', function(e) {
    chrome.test.sendMessage('guest-loaded');
  });

  window.removeWebviewOnExitDoCrash = function() {
    webview.terminate();
  };

  webview.addEventListener('exit', function(e) {
    // We expected to be killed.
    if (e.reason != 'killed') {
      console.log('EXPECTED TO BE KILLED!');
      return;
    }
    webview.parentNode.removeChild(webview);
  });

  // Trigger a navigation to create a guest process.
  webview.setAttribute('src', embedder.emptyGuestURL);
  document.body.appendChild(webview);
}

function testRemoveWebviewAfterNavigation() {
  var webview = new WebView();
  document.body.appendChild(webview);
  webview.src = 'data:text/html,trigger navigation';
  document.body.removeChild(webview);
  setTimeout(function() {
    embedder.test.succeed();
  }, 0);
}

function testNavigationToExternalProtocol() {
  var webview = document.createElement('webview');
  webview.addEventListener('loadstop', function(e) {
    webview.addEventListener('loadabort', function(e) {
      embedder.test.assertEq('ERR_UNKNOWN_URL_SCHEME', e.reason);
      embedder.test.succeed();
    });
    webview.executeScript({
      code: 'window.location.href = "tel:+12223334444";'
    }, function(results) {});
  });
  webview.setAttribute('src', 'data:text/html,navigate to external protocol');
  document.body.appendChild(webview);
}

// This test ensures if the guest isn't there and we resize the guest (from JS),
// it remembers the size correctly.
function testNavigateAfterResize() {
  var webview = new WebView();

  var postMessageHandler = function(e) {
    var data = JSON.parse(e.data);
    LOG('postMessageHandler: ' + data);
    webview.removeEventListener('message', postMessageHandler);
    if (data[0] == 'dimension-response') {
      var actualWidth = data[1];
      var actualHeight = data[2];
      LOG('actualWidth: ' + actualWidth + ', actualHeight: ' + actualHeight);
      embedder.test.assertEq(100, actualWidth);
      embedder.test.assertEq(125, actualHeight);
      embedder.test.succeed();
    }
  };
  window.addEventListener('message', postMessageHandler);

  webview.addEventListener('consolemessage', function(e) {
    LOG('guest log: ' + e.message);
  });

  webview.addEventListener('loadstop', function(e) {
    webview.executeScript(
      {file: 'navigate_after_resize.js'},
      function(results) {
        if (!results || !results.length) {
          LOG('Failed to inject navigate_after_resize.js');
          embedder.test.fail();
          return;
        }
        LOG('Inject success: navigate_after_resize.js');
        var msg = ['dimension-request'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
  });

  // First set size.
  webview.style.width = '100px';
  webview.style.height = '125px';

  // Then navigate.
  webview.src = 'about:blank';
  document.body.appendChild(webview);
}

function testResizeWebviewResizesContent() {
  var webview = new WebView();
  webview.src = 'about:blank';
  webview.addEventListener('loadstop', function(e) {
    webview.executeScript(
      {file: 'inject_resize_test.js'},
      function(results) {
        window.console.log('The resize test has been injected into webview.');
      }
    );
    webview.executeScript(
      {file: 'inject_comm_channel.js'},
      function(results) {
        window.console.log('The guest script for a two-way comm channel has ' +
            'been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      }
    );
  });
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
      console.log('Resizing <webview> width from 300px to 400px.');
      webview.style.width = '400px';
      return;
    }
    if (data[0] == 'resize') {
      var width = data[1];
      var height = data[2];
      // If the 'resize' event was because of the initial size, ignore it.
      if (width == 300 && height == 300) {
        return;
      }
      embedder.test.assertEq(400, width);
      embedder.test.assertEq(300, height);
      embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });
  document.body.appendChild(webview);
}

function testResizeWebviewWithDisplayNoneResizesContent() {
  var webview = new WebView();
  webview.src = 'about:blank';
  var loadStopCalled = false;
  webview.addEventListener('loadstop', function listener(e) {
    if (loadStopCalled) {
      window.console.log('webview is unexpectedly reloading.');
      embedder.test.fail();
      return;
    }
    loadStopCalled = true;
    webview.executeScript(
      {file: 'inject_resize_test.js'},
      function(results) {
        if (!results || !results.length) {
          embedder.test.fail();
          return;
        }
        window.console.log('The resize test has been injected into webview.');
      }
    );
    webview.executeScript(
      {file: 'inject_comm_channel.js'},
      function(results) {
        if (!results || !results.length) {
          embedder.test.fail();
          return;
        }
        window.console.log('The guest script for a two-way comm channel has ' +
            'been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      }
    );
  });
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
      console.log('Resizing <webview> width from 300px to 400px.');
      webview.style.display = 'none';
      window.setTimeout(function() {
        webview.style.width = '400px';
        window.setTimeout(function() {
          webview.style.display = 'block';
        }, 10);
      }, 10);
      return;
    }
    if (data[0] == 'resize') {
      var width = data[1];
      var height = data[2];
      // If the 'resize' event was because of the initial size, ignore it.
      if (width == 300 && height == 300) {
        return;
      }
      embedder.test.assertEq(400, width);
      embedder.test.assertEq(300, height);
      embedder.test.succeed();
      return;
    }
    window.console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });
  document.body.appendChild(webview);
}

function testPostMessageCommChannel() {
  var webview = new WebView();
  // Run this test with display:none to verify that postMessage works correctly.
  webview.style.display = 'none';
  webview.src = 'about:blank';
  webview.addEventListener('loadstop', function(e) {
    window.console.log('loadstop');
    webview.executeScript(
      {file: 'inject_comm_channel.js'},
      function(results) {
        window.console.log('The guest script for a two-way comm channel has ' +
            'been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      }
    );
  });
  webview.addEventListener('consolemessage', function(e) {
    window.console.log('Guest: "' + e.message + '"');
  });
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
      embedder.test.succeed();
      return;
    }
    console.log('Unexpected message: \'' + data[0]  + '\'');
    embedder.test.fail();
  });
  document.body.appendChild(webview);
}

function testScreenshotCapture() {
  var webview = document.createElement('webview');

  webview.addEventListener('loadstop', function(e) {
    webview.captureVisibleRegion(null, function(dataUrl) {
      // 100x100 red box.
      var expectedUrl = 'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/' +
          '2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLE' +
          'BYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFB' +
          'QUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCABkAGQ' +
          'DASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAA' +
          'AgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM' +
          '2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3' +
          'R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8j' +
          'JytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAA' +
          'AAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBU' +
          'QdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERU' +
          'ZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqO' +
          'kpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3' +
          '+Pn6/9oADAMBAAIRAxEAPwD50ooor8MP9UwooooAKKKKACiiigAooooAKKKKACiii' +
          'gAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiig' +
          'AooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigA' +
          'ooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAo' +
          'oooAKKKKACiiigAooooAKKKKACiiigD/2Q==';
      embedder.test.assertEq(expectedUrl, dataUrl);
      embedder.test.succeed();
    });
  });

  webview.style.width = '100px';
  webview.style.height = '100px';
  webview.setAttribute('src',
      'data:text/html,<body style="background-color: red"></body>');
  document.body.appendChild(webview);
}

function getWebviewInnerWidth(webview) {
  // Double rAF to help avoid flakiness if an update that affects layout hasn't
  // happened yet.
  let getInnerWidthAfterLifecycleUpdate = () => {
    return new Promise((resolve) => {
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          resolve(window.innerWidth);
        });
      });
    });
  };
  return evalInWebView(webview, getInnerWidthAfterLifecycleUpdate, []);
}

function testZoomAPI() {
  var webview = new WebView();
  webview.src = 'about:blank';
  webview.addEventListener('loadstop', async function(e) {
    // getZoom() should work initially.
    embedder.test.assertEq(await getZoomP(webview), 1);

    // Two consecutive calls to getZoom() should return the same result.
    let results = await Promise.all([getZoomP(webview), getZoomP(webview)]);
    embedder.test.assertEq(results[0], results[1]);

    // Test setZoom()'s callback.
    await setZoomP(webview, 0.95);

    // getZoom() should return the same zoom factor as is set in setZoom().
    webview.setZoom(1.53);
    embedder.test.assertEq(await getZoomP(webview), 1.53);
    webview.setZoom(0.835847);
    embedder.test.assertEq(await getZoomP(webview), 0.835847);
    webview.setZoom(0.3795);
    embedder.test.assertEq(await getZoomP(webview), 0.3795);

    // setZoom() should really zoom the page (thus changing window.innerWidth).
    await setZoomP(webview, 0.45);
    let width1 = await getWebviewInnerWidth(webview);

    await setZoomP(webview, 1.836);
    let width2 = await getWebviewInnerWidth(webview);
    embedder.test.assertTrue(width2 < width1);

    await setZoomP(webview, 0.73);
    let width3 = await getWebviewInnerWidth(webview);
    embedder.test.assertTrue(width3 < width1);
    embedder.test.assertTrue(width2 < width3);

    // Test the onzoomchange event.
    webview.addEventListener('zoomchange', (event) => {
      embedder.test.assertEq(event.oldZoomFactor, 0.73);
      embedder.test.assertEq(event.newZoomFactor, 0.25325);

      embedder.test.succeed();
    });
    webview.setZoom(0.25325);
  });
  document.body.appendChild(webview);
};

var testFindPage =
    'data:text/html,Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br>' +
    'Dog dog dog Dog dog dogcatDog dogDogdog.<br><br>' +
    '<a href="about:blank">Click here!</a>';

function testFindAPI() {
  var webview = new WebView();
  webview.src = testFindPage;

  var loadstopListener2 = function(e) {
    embedder.test.assertEq(webview.src, "about:blank");
    // Test find results when looking for nothing.
    webview.find("", {}, function(results) {
      embedder.test.assertEq(results.numberOfMatches, 0);
      embedder.test.assertEq(results.activeMatchOrdinal, 0);
      embedder.test.assertEq(results.selectionRect.left, 0);
      embedder.test.assertEq(results.selectionRect.top, 0);
      embedder.test.assertEq(results.selectionRect.width, 0);
      embedder.test.assertEq(results.selectionRect.height, 0);

      embedder.test.succeed();
    });
  }

  var loadstopListener1 = function(e) {
    // Test find results.
    webview.find("dog", {}, function(results) {
      embedder.test.assertEq(results.numberOfMatches, 100);
      embedder.test.assertTrue(results.selectionRect.width > 0);
      embedder.test.assertTrue(results.selectionRect.height > 0);

      // Test finding next active matches.
      webview.find("dog");
      webview.find("dog");
      webview.find("dog");
      webview.find("dog");
      webview.find("dog", {}, function(results) {
        embedder.test.assertEq(results.activeMatchOrdinal, 6);
        webview.find("dog", {backward: true});
        webview.find("dog", {backward: true}, function(results) {
          // Test the |backward| find option.
          embedder.test.assertEq(results.activeMatchOrdinal, 4);

          // Test the |matchCase| find option.
          webview.find("Dog", {matchCase: true}, function(results) {
            embedder.test.assertEq(results.numberOfMatches, 40);

            // Test canceling find requests.
            webview.find("dog");
            webview.stopFinding();
            webview.find("dog");
            webview.find("cat");

            // Test find results when looking for something that isn't there.
            webview.find("fish", {}, function(results) {
              embedder.test.assertEq(results.numberOfMatches, 0);
              embedder.test.assertEq(results.activeMatchOrdinal, 0);
              embedder.test.assertEq(results.selectionRect.left, 0);
              embedder.test.assertEq(results.selectionRect.top, 0);
              embedder.test.assertEq(results.selectionRect.width, 0);
              embedder.test.assertEq(results.selectionRect.height, 0);

              // Test following a link with stopFinding().
              webview.removeEventListener('loadstop', loadstopListener1);
              webview.addEventListener('loadstop', loadstopListener2);
              webview.find("click here!", {}, function() {
                webview.stopFinding("activate");
              });
            });
          });
        });
      });
    });
  };

  webview.addEventListener('loadstop', loadstopListener1);
  document.body.appendChild(webview);
};

// TODO(paulmeyer): Make sure this test is not still flaky. If it is, it is
// likely because the search for "dog" compelted too quickly. crbug.com/710486.
function testFindAPI_findupdate() {
  var webview = new WebView();
  webview.src = testFindPage;

  var canceledTest = false;
  webview.addEventListener('loadstop', function(e) {
    // Test the |findupdate| event.
    webview.addEventListener('findupdate', function(e) {
      if (e.activeMatchOrdinal > 0) {
        embedder.test.assertTrue(e.numberOfMatches >= e.activeMatchOrdinal)
        embedder.test.assertTrue(e.selectionRect.width > 0);
        embedder.test.assertTrue(e.selectionRect.height > 0);
      }

      if (e.finalUpdate) {
        if (e.canceled) {
          canceledTest = true;
        } else {
          embedder.test.assertEq(e.searchText, "cat");
          embedder.test.assertEq(e.numberOfMatches, 10);
          embedder.test.assertEq(e.activeMatchOrdinal, 1);
          embedder.test.assertTrue(canceledTest);
          embedder.test.succeed();
        }
      }
    });
    webview.find("dog");
    webview.find("dog");
    webview.find("cat");
  });

  document.body.appendChild(webview);
};

function testFindInMultipleWebViews() {
  var webviews = [new WebView(), new WebView(), new WebView()];
  var promises = [];

  // Search in all WebViews simultaneously.
  for (var i in webviews) {
    webviews[i].src = testFindPage;
    promises[i] = new Promise((resolve, reject) => {
      webviews[i].addEventListener('loadstop', function(id, event) {
        LOG("Searching WebView " + id + ".");

        var webview = webviews[id];
        webview.find("dog", {}, (results_a) => {
          embedder.test.assertEq(results_a.numberOfMatches, 100);
          embedder.test.assertTrue(results_a.selectionRect.width > 0);
          embedder.test.assertTrue(results_a.selectionRect.height > 0);

          // Test finding next active matches.
          webview.find("dog");
          webview.find("dog");
          webview.find("dog");
          webview.find("dog");
          webview.find("dog", {}, (results_b) => {
            embedder.test.assertEq(results_b.activeMatchOrdinal, 6);
            LOG("Searched WebView " + id + " successfully.");
            resolve();
          });
        });
      }.bind(undefined, i));
    });
    document.body.appendChild(webviews[i]);
  }

  Promise.all(promises)
      .then(() => {
        LOG("All searches finished.");
        embedder.test.succeed();
      })
      .catch((error) => {
        LOG("Failing test.");
        embedder.test.fail(error);
      });
}

function testFindAfterTerminate() {
  let webview = new WebView();
  webview.src = 'data:text/html,<body><iframe></iframe></body>';
  webview.addEventListener('loadstop', () => {
    webview.find('A');
    webview.terminate();
    webview.find('B', {'backward': true});
    webview.find('B', {'backward': true}, (results) => {
      embedder.test.succeed();
    });
  });
  document.body.appendChild(webview);
}

function testLoadDataAPI() {
  var webview = new WebView();
  webview.src = 'about:blank';

  const virtualURL = 'http://virtualurl/';

  var loadstopListener2 = function(e) {
    // Test the virtual URL.
    embedder.test.assertEq(webview.src, virtualURL);

    // Test that the image was loaded from the right source.
    webview.executeScript(
        {code: "document.querySelector('img').src"}, function(e) {
          embedder.test.assertEq(e, embedder.testImageBaseURL + "test.bmp");

          // Test that insertCSS works (executeScript already works to reach
          // this point).
          webview.insertCSS({code: ''}, function() {
            embedder.test.succeed();
          });
        });
  };

  var loadstopListener1 = function(e) {
    webview.removeEventListener('loadstop', loadstopListener1);
    webview.addEventListener('loadstop', loadstopListener2);

    // Load a data URL containing a relatively linked image, with the
    // image's base URL specified, and a virtual URL provided.
    let encodedData =
        window.btoa('<html>This is a test.<br><img src="test.bmp"><br></html>');
    webview.loadDataWithBaseUrl("data:text/html;base64," + encodedData,
                                embedder.testImageBaseURL,
                                virtualURL);
  };

  webview.addEventListener('loadstop', loadstopListener1);
  document.body.appendChild(webview);
}

// loadDataWithBaseUrl cannot generally be used with a chrome-extension:// base
// URL, however the embedding extension may use its own chrome-extension://
// origin. We test that an embedder can use its own origin as the base and that
// relative URLs resolve to it by loading something in the guest from the
// embedder's accessible_resources.
function testLoadDataAPIAccessibleResources() {
  let webview = new WebView();
  // The accessible_resources listed in the manifest file are under the
  // "foobar" partition.
  webview.partition = 'foobar';
  webview.src = 'about:blank';

  let loadstopListener2 = function() {
    webview.executeScript(
        {code: 'document.querySelector(\'img\').src'}, (e) => {
          embedder.test.assertEq(e, location.origin + '/test.bmp');
          embedder.test.succeed();
        });
  };

  let loadstopListener1 = function() {
    webview.removeEventListener('loadstop', loadstopListener1);
    webview.addEventListener('loadstop', loadstopListener2);

    let encodedData =
        window.btoa('<html>This is a test.<br><img src="test.bmp"><br></html>');
    webview.loadDataWithBaseUrl('data:text/html;base64,' + encodedData,
                                location.origin);
  };

  webview.addEventListener('loadstop', loadstopListener1);
  document.body.appendChild(webview);
}

// Test that the resize events fire with the correct values, and in the
// correct order, when resizing occurs.
function testResizeEvents() {
  var webview = new WebView();
  webview.src = 'about:blank';
  webview.style.width = '600px';
  webview.style.height = '400px';

  var loadstopListener = function(e) {
    webview.removeEventListener('loadstop', loadstopListener);

    // Observer to look for window.resize event inside <webview>.
    webview.onconsolemessage = function(e) {
      if (e.message === 'ONRESIZE: 500X400') {
        embedder.test.succeed();
      }
    };

    webview.executeScript(
        {
           code: 'window.onresize=function(){' +
                 '  console.log("ONRESIZE: " + window.innerWidth + "X" +' +
                 '              window.innerHeight);' +
                 '}'
        }, function(results) {
          if (!results || !results.length) {
            embedder.test.fail();
            return;
          }
          console.log('Resizing <webview> width from 600px to 500px.');
          webview.style.width = '500px';
        });
  };

  webview.addEventListener('loadstop', loadstopListener);
  document.body.appendChild(webview);
};

function testPerOriginZoomMode() {
  var webview1 = new WebView();
  var webview2 = new WebView();
  webview1.src = 'about:blank';
  webview2.src = 'about:blank';

  webview1.addEventListener('loadstop', function(e) {
    document.body.appendChild(webview2);
  });
  webview2.addEventListener('loadstop', function(e) {
    webview1.getZoomMode(async function(zoomMode) {
      // Check that |webview1| is in 'per-origin' mode and zoom it. Check that
      // both webviews zoomed.
      embedder.test.assertEq(zoomMode, 'per-origin');

      let width1Before = await getWebviewInnerWidth(webview1);
      let width2Before = await getWebviewInnerWidth(webview2);

      await setZoomP(webview1, 3.14);
      embedder.test.assertEq(await getZoomP(webview1), 3.14);
      embedder.test.assertEq(await getZoomP(webview2), 3.14);

      let width1After = await getWebviewInnerWidth(webview1);
      let width2After = await getWebviewInnerWidth(webview2);

      embedder.test.assertTrue(width1After < width1Before);
      embedder.test.assertTrue(width2After < width2Before);

      embedder.test.succeed();
    });
  });

  document.body.appendChild(webview1);
}

function testPerViewZoomMode() {
  var webview1 = new WebView();
  var webview2 = new WebView();
  webview1.src = 'about:blank';
  webview2.src = 'about:blank';

  webview1.addEventListener('loadstop', function(e) {
    document.body.appendChild(webview2);
  });
  webview2.addEventListener('loadstop', async function(e) {
    // Set |webview2| to 'per-view' mode and zoom it. Make sure that the
    // zoom did not affect |webview1|.
    // We need to verify that the page actually is zooming by comparing
    // |window.innerWidth| before and after the zoom to prevent regressions like
    // https://crbug.com/860511.
    let webview1_original_width = await getWebviewInnerWidth(webview1);
    let webview2_original_width = await getWebviewInnerWidth(webview2);

    webview2.setZoomMode('per-view', function() {
      webview2.getZoomMode(async function(zoomMode) {
        embedder.test.assertEq(zoomMode, 'per-view');

        await setZoomP(webview2, 0.45);
        embedder.test.assertFalse((await getZoomP(webview1)) == 0.45);
        embedder.test.assertEq(await getZoomP(webview2), 0.45);

        let webview1_new_width = await getWebviewInnerWidth(webview1);
        // Verify that inner width has not been changed for
        // for this WebView.
        embedder.test.assertEq(webview1_new_width, webview1_original_width);

        let webview2_new_width = await getWebviewInnerWidth(webview2);
        // Verify that inner width has been updated for
        // the second WebView.
        embedder.test.assertTrue(webview2_original_width < webview2_new_width);
        embedder.test.succeed();
      });
    });
  });

  document.body.appendChild(webview1);
}

function testDisabledZoomMode() {
  var webview = new WebView();
  webview.src = 'about:blank';

  var zoomchanged = false;
  var zoomchangeListener = function(e) {
    embedder.test.assertEq(e.newZoomFactor, 1);
    zoomchanged = true;
  };

  webview.addEventListener('loadstop', function(e) {
    // Set |webview| to 'disabled' mode and check that
    // zooming is actually disabled. Also check that the
    // "zoomchange" event pick up changes from changing the
    // zoom mode.
    webview.addEventListener('zoomchange', zoomchangeListener);
    webview.setZoomMode('disabled', function() {
      webview.getZoomMode(function(zoomMode) {
        embedder.test.assertEq(zoomMode, 'disabled');
        webview.removeEventListener('zoomchange', zoomchangeListener);
        webview.setZoom(1.39, function() {
          webview.getZoom(function(zoom) {
            embedder.test.assertEq(zoom, 1);
            embedder.test.assertTrue(zoomchanged);
            embedder.test.succeed();
          });
        });
      });
    });
  });

  document.body.appendChild(webview);
}

function testZoomBeforeNavigation() {
  var webview = new WebView();

  webview.addEventListener('loadstop', async function(e) {
    // Check that the zoom state persisted.
    embedder.test.assertEq(await getZoomP(webview), 3.14);

    // Disable zoom so that the webview reverts to the default zoom level. We
    // then verify that there was a difference in layout when the zoom was
    // applied.
    let width1 = await getWebviewInnerWidth(webview);
    webview.setZoomMode('disabled', async function() {
      let width2 = await getWebviewInnerWidth(webview);
      embedder.test.assertTrue(width2 > width1);

      embedder.test.succeed();
    });
  });

  // Set the zoom before the first navigation.
  webview.setZoom(3.14);

  webview.src = 'about:blank';
  document.body.appendChild(webview);
}

function testPlugin() {
  var webview = document.createElement('webview');
  webview.setAttribute('src', embedder.pluginURL);
  webview.addEventListener('loadstop', function(e) {
    // Not crashing means success.
    embedder.test.succeed();
  });
  document.body.appendChild(webview);
}

function testGarbageCollect() {
  let webview = new WebView();
  webview = null;

  window.gc({type: 'major', execution: 'async'}).then(() => {
    embedder.test.succeed();
  });
}

// This test verifies that when an app window is closed, only the state for the
// webview in that window is cleaned up, and not the entire embedder process.
function testCloseNewWindowCleanup() {
  chrome.app.window.create('appwindow.html', {
    width: 640,
    height: 480
  }, function(appWindow) {
    appWindow.contentWindow.onload = function(evt) {
      var webview = appWindow.contentWindow.document.querySelector('webview');
      webview.addEventListener('loadstop', function(evt) {
        // Close the second app window, which should not trigger the whole
        // embedder process to be cleaned up.
        appWindow.contentWindow.close();
        embedder.test.succeed();
      });
      webview.src = 'about:blank';
    };
  });
}

function testFocusWhileFocused() {
  var webview = new WebView();
  webview.src = 'about:blank';

  webview.addEventListener('loadstop', function(e) {
    // Focus twice, then make sure that the internal element is still focused.
    webview.focus();
    webview.focus();
    embedder.test.assertEq(document.activeElement, webview);
    var webviewPrivates =
        chrome.test.getModuleSystem(webview).privates(webview);
    var shadowRoot = webviewPrivates.internal.shadowRoot;
    embedder.test.assertTrue(shadowRoot.activeElement);
    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

function testPDFInWebview() {
  var webview = document.createElement('webview');
  var pdfUrl = 'test.pdf';
  // partition 'foobar' has access to local resource |pdfUrl|.
  webview.partition = 'foobar';
  webview.onloadstop = embedder.test.succeed;
  webview.onloadabort = embedder.test.fail;
  webview.setAttribute('src', pdfUrl);
  document.body.appendChild(webview);
}

function testNavigateToPDFInWebview() {
  var webview = document.createElement('webview');
  var pdfUrl = 'test.pdf';
  // partition 'foobar' has access to local resource |pdfUrl|.
  webview.partition = 'foobar';
  webview.onloadabort = embedder.test.fail;

  var loadstopHandler = function(e) {
    webview.removeEventListener('loadstop', loadstopHandler);
    webview.addEventListener('loadstop', embedder.test.succeed);
    webview.setAttribute('src', pdfUrl);
  };
  webview.addEventListener('loadstop', loadstopHandler);

  webview.setAttribute('src', 'about:blank');
  document.body.appendChild(webview);
}

// Test that when a PDF loaded in a webview triggers a JS dialog, the webview's
// embedder receives the request.
function testDialogInPdf() {
  let webview = document.createElement('webview');
  let pdfUrl = 'pdf_with_dialog.pdf';
  // Partition 'foobar' has access to local resource |pdfUrl|.
  webview.partition = 'foobar';
  webview.src = pdfUrl;
  webview.addEventListener('dialog', (e) => {
    e.dialog.ok();
    embedder.test.succeed();
  });
  document.body.appendChild(webview);
}

// This test verifies that mailto links are enabled.
function testMailtoLink() {
  var webview = new WebView();
  webview.src = embedder.mailtoTestURL;

  webview.onloadstop = function() {
    webview.onloadabort = function(e) {
      // The mailto link should not trigger a loadabort.
      if (e.url.substring(0, 7) == 'mailto:') {
        embedder.test.fail();
      }
    };
    webview.onloadstop = function() {
      // If mailto links are disabled, then |webview.src| will now be
      // 'about:blank'.
      embedder.test.assertFalse(webview.src == 'about:blank');
      embedder.test.succeed();
    };
    webview.executeScript({code:'document.getElementById("mailto").click()'});
  };

  document.body.appendChild(webview);
}

// This test verifies that an embedder can navigate a WebView to a blob URL it
// creates. Additionally this test also verifies that an embedder can't navigate
// to a blob URL a WebView creates.
function testBlobURL() {
  var webview = new WebView();
  var blob =
      new Blob([`
<html>
  <body>Blob content</body>
  <script>
    self.onmessage = e => {
      const blob = new Blob(['hello world']);
      const url = URL.createObjectURL(blob);
      e.ports[0].postMessage(url);
    };
  </script>
</html>`], {type: 'text/html'});
  var blobURL = URL.createObjectURL(blob);
  webview.src = blobURL;

  webview.onloadabort = function() {
    // The blob: URL load should not trigger a loadabort.
    window.console.log('Blob URL load was aborted.');
    embedder.test.fail();
  };
  webview.onloadstop = function() {
    embedder.test.assertTrue(webview.src == blobURL);
    const channel = new MessageChannel();
    webview.contentWindow.postMessage('', '*', [channel.port1]);
    channel.port2.onmessage = e => {
      embedder.test.assertTrue(e.data.startsWith('blob:' + window.origin));
      fetch(e.data).then(
          () => {
            window.console.log('Blob URL load incorrectly succeeded.');
            embedder.test.fail();
          },
          () => {
            embedder.test.succeed();
          });
    };
  };

  document.body.appendChild(webview);
}

// This test navigates an unattached guest to 'about:blank', then it makes a
// renderer/ navigation to a URL that results in a server side redirect. In the
// end we verify that the redirected URL loads in the guest properly.
function testRendererNavigationRedirectWhileUnattached() {
  var webview = document.createElement('webview');
  // So that |webview| is unattached, but can navigate.
  webview.style.display = 'none';

  var seenRedirectURLCommit = false;
  var seenRedirectLoadStop = false;

  var checkTest = function() {
    if (seenRedirectLoadStop && seenRedirectURLCommit) {
      embedder.test.succeed();
    }
  };

  webview.onloadstop = function(e) {

    webview.onloadstop = function() {
      webview.onloadstop = null;
      seenRedirectLoadStop = true;
      checkTest();
    };
    webview.executeScript({
      code: 'window.location.href="' + embedder.redirectGuestURL + '"',
    }, function(res) {
      if (!res || !res.length) {
        embedder.test.fail();
        return;
      }
    });
  };

  webview.onloadcommit = function(e) {
    if (e.url.indexOf(GUEST_REDIRECT_FILE_NAME) != -1) {
      seenRedirectURLCommit = true;
      checkTest();
    }
  };
  document.body.appendChild(webview);
  webview.src = 'about:blank';
};

function testRemoveBeforeAttach() {
  // Create a guest and immediately remove it. So once the browser acknowledges
  // creation, the guest will be destroyed on the renderer side and no
  // attachment request will occur.
  let webview = document.createElement('webview');
  webview.src = 'about:blank';
  document.body.appendChild(webview);
  webview.remove();

  embedder.test.succeed();
};

function runNewWindowCrossWindowAttachTest(noopener) {
  let firstWebviewUrl = noopener ? embedder.windowOpenNoopenerGuestURL :
                                   embedder.windowOpenGuestURL;
  let webview = document.createElement('webview');
  webview.src = firstWebviewUrl;

  async function checkOpenerRelationships(secondWebview) {
    let hasOpenerResult =
        await executeScriptP(secondWebview, {code: '!!window.opener;'});
    embedder.test.assertEq(1, hasOpenerResult.length);
    embedder.test.assertEq(!noopener, hasOpenerResult[0]);

    if (!noopener) {
      let openerUsageResult = await executeScriptP(
          secondWebview, {code: 'window.opener.location.href;'});
      embedder.test.assertEq(1, openerUsageResult.length);
      embedder.test.assertEq(firstWebviewUrl, openerUsageResult[0]);

      // The first webview should be able to get, by name, another window
      // reference to the window it previously opened.
      let refFromNameResult = await executeScriptP(
          webview,
          {code: 'window.open(\'\', \'namedWebview\').location.href;'});
      embedder.test.assertEq(1, refFromNameResult.length);
      embedder.test.assertEq(embedder.emptyGuestURL, refFromNameResult[0]);
    }

    // After this test exits, we'll still need to compare embedders in the
    // C++ part of this test.
    embedder.test.succeed();
  }

  webview.addEventListener('newwindow', (e) => {
    e.preventDefault();
    let secondAppWindowUrl = 'new_window_main.html';
    chrome.app.window.create(secondAppWindowUrl, {}, function(app_new_window) {
      if (chrome.runtime.lastError) {
        console.log('Error:' + chrome.runtime.lastError.message);
        embedder.test.fail();
        return;
      }

      let new_window = app_new_window.contentWindow;
      new_window.onload = () => {
        let new_webview = new_window.document.createElement('webview');
        new_webview.addEventListener('loadstop', () => {
          if (new_webview.src == embedder.emptyGuestURL) {
            checkOpenerRelationships(new_webview);
          }
        });
        // Be sure to do the attach before appending to document.
        e.window.attach(new_webview);
        new_window.document.body.appendChild(new_webview);
      };
    });
  });
  document.body.appendChild(webview);
}

function testWebViewAndEmbedderInNewWindow() {
  runNewWindowCrossWindowAttachTest(false);
}

function testWebViewAndEmbedderInNewWindow_Noopener() {
  runNewWindowCrossWindowAttachTest(true);
}

function testNewWindowNoDeadlock() {
  let webview = document.createElement('webview');
  let newwindowEvent = null;
  webview.addEventListener('loadstop', () => {
    // First, we send a message to the guest, which will perform a window.open.
    webview.contentWindow.postMessage('', '*');
  });
  webview.addEventListener('newwindow', (e) => {
    // Once the guest calls window.open, we receive the request here.
    // However, we postpone the attachment until we get a message back from the
    // guest. The implementation cannot delay responding to the sync window.open
    // IPC until attachment, because the message handler below performs the
    // attachment, and that does not run until the guest's window.open call
    // returns and it sends a message back to this embedder.
    e.preventDefault();
    newwindowEvent = e;
  });
  window.addEventListener('message', (e) => {
    let newwebview = document.createElement('webview');
    newwindowEvent.window.attach(newwebview);
    document.body.appendChild(newwebview);
    embedder.test.succeed();
  });
  webview.src = embedder.windowOpenMessageURL;
  document.body.appendChild(webview);
}

function testSelectPopupPositionInMac() {
  var webview = document.createElement('webview');
  webview.id = 'popup-test-mac';
  webview.partition = 'foobar';
  // Offset the <webview> location inside the app so that the corner is almost
  // 250 pixels off from app window's origin.
  webview.style = 'position: fixed; left: 240px; top: 70px; border: solid;';
  webview.addEventListener('loadstop', function() {
    // This lets the browser know that it can start sending down input events
    // for the remainder of the test.
    embedder.test.succeed();
  });

  webview.setAttribute('src', chrome.runtime.getURL('guest_with_select.html'));
  document.body.appendChild(webview);
}

function testWebRequestBlockedNavigation() {
  var webview = new WebView();
  webview.addEventListener('loadcommit', (e) => {
    // Cancel all subsequent requests.
    webview.request.onHeadersReceived.addListener(() => {
      return {cancel: true};
    }, {urls: ['<all_urls>']}, ['blocking']);

    // TODO(mcnee): Consider testing that a 'loadabort' event is fired as well.

    // When a request is cancelled, it will eventually fire a loadstop event. If
    // webview hasn't been navigated away from the echo page to an error page,
    // it will echo MessageEvent.data back.
    webview.addEventListener('loadstop', () => {
      // Note: simply checking `src` doesn't work here, since it's set to the
      // URL of the last attempted navigation (which was blocked).
      // TODO(crbug.com/40718552): Clarify/figure out how the src
      // attribute should behave.
      webview.contentWindow.postMessage('moo', '*');
    });
    window.addEventListener('message', (e) => {
      embedder.test.assertEq('moo', e.data);
      embedder.test.succeed();
    });
    webview.src = embedder.emptyGuestURL;
  });
  webview.src = embedder.echoURL;
  document.body.appendChild(webview);
}

function testBlankWebview() {
  var webview = new WebView();
  webview.src = "about:blank";
  document.body.appendChild(webview);
  webview.addEventListener('loadstop', function() {
    // This lets the browser know that it can start sending down input events
    // for the remainder of the test.
    embedder.test.succeed();
  });
}

function testAddFencedFrame() {
  let fencedFrameHostURL = embedder.baseGuestURL +
      '/extensions/platform_apps/web_view/shim/fenced_frame_host.html';

  let webview = new WebView();
  webview.src = fencedFrameHostURL;
  webview.addEventListener('loadstop', () => {
    embedder.test.succeed();
  });
  document.body.appendChild(webview);
}

// This test and several tests below test scenarios where a webview element is
// created and/or attached by different documents. In this test, we create a
// webview element with the main frame's document, but embed it in an iframe's
// document.
function testInsertIntoIframe() {
  let webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  let iframe = document.createElement('iframe');
  iframe.src = 'empty.html';

  webview.addEventListener('loadstop', () => {
    embedder.test.succeed();
  });

  iframe.addEventListener('load', () => {
    iframe.contentDocument.body.appendChild(webview);
  });

  document.body.appendChild(iframe);
}

// See testInsertIntoIframe.
// Here an iframe both creates and embeds the webview element.
function testCreateAndInsertInIframe() {
  let iframe = document.createElement('iframe');
  iframe.src = 'empty.html';

  iframe.addEventListener('load', () => {
    let webview = iframe.contentDocument.createElement('webview');
    webview.src = embedder.emptyGuestURL;
    webview.addEventListener('loadstop', () => {
      embedder.test.succeed();
    });

    iframe.contentDocument.body.appendChild(webview);
  });

  document.body.appendChild(iframe);
}

// See testInsertIntoIframe.
// Here an iframe creates a webview element, but embeds it in the main document.
function testInsertIntoMainFrameFromIframe() {
  let iframe = document.createElement('iframe');
  iframe.src = 'empty.html';

  iframe.addEventListener('load', () => {
    let webview = iframe.contentDocument.createElement('webview');
    webview.src = embedder.emptyGuestURL;
    webview.addEventListener('loadstop', () => {
      embedder.test.succeed();
    });

    document.body.appendChild(webview);
  });

  document.body.appendChild(iframe);
}

// See testInsertIntoIframe.
// Here this document creates a webview element, but embeds it in another app
// window.
function testInsertIntoOtherWindow() {
  let webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;

  webview.addEventListener('loadstop', () => {
    embedder.test.succeed();
  });

  webview.addEventListener('loadabort', () => {
    embedder.test.fail();
  });

  chrome.app.window.create('new_window_main.html', {}, (app_new_window) => {
    if (chrome.runtime.lastError) {
      console.log('Error:' + chrome.runtime.lastError.message);
      embedder.test.fail();
      return;
    }

    let new_window = app_new_window.contentWindow;
    new_window.addEventListener('load', () => {
      new_window.document.body.appendChild(webview);
    });
  });
}

// See testInsertIntoIframe.
// Here another app window both creates and embeds the webview element.
function testCreateAndInsertInOtherWindow() {
  chrome.app.window.create('new_window_main.html', {}, (app_new_window) => {
    if (chrome.runtime.lastError) {
      console.log('Error:' + chrome.runtime.lastError.message);
      embedder.test.fail();
      return;
    }

    let new_window = app_new_window.contentWindow;
    new_window.addEventListener('load', () => {
      let webview = new_window.document.createElement('webview');
      webview.src = embedder.emptyGuestURL;
      webview.addEventListener('loadstop', () => {
        embedder.test.succeed();
      });
      webview.addEventListener('loadabort', () => {
        embedder.test.fail();
      });

      new_window.document.body.appendChild(webview);
    });
  });
}

// See testInsertIntoIframe.
// Here another app window creates a webview element, but embeds it in this
// document.
function testInsertFromOtherWindow() {
  chrome.app.window.create('new_window_main.html', {}, (app_new_window) => {
    if (chrome.runtime.lastError) {
      console.log('Error:' + chrome.runtime.lastError.message);
      embedder.test.fail();
      return;
    }

    let new_window = app_new_window.contentWindow;
    new_window.addEventListener('load', () => {
      let webview = new_window.document.createElement('webview');
      webview.src = embedder.emptyGuestURL;
      webview.addEventListener('loadstop', () => {
        embedder.test.succeed();
      });
      webview.addEventListener('loadabort', () => {
        embedder.test.fail();
      });

      document.body.appendChild(webview);
    });
  });
}

// Inserting a webview element into a detached iframe's document shouldn't
// crash.
function testInsertIntoDetachedIframe() {
  let webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  let iframe = document.createElement('iframe');

  iframe.addEventListener('load', () => {
    let doc = iframe.contentDocument;
    iframe.remove();
    doc.body.appendChild(webview);

    setTimeout(() => {
      embedder.test.succeed();
    });
  });

  document.body.appendChild(iframe);
}

function testCannotRequestUsb() {
  let webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  webview.addEventListener('loadstop', async () => {
    let getUsbDevices = async () => {
      let devices = await navigator.usb.getDevices();
      return devices.map(device => device.serialNumber);
    };
    let requestUsbDevice = async () => {
      let device = await navigator.usb.requestDevice({filters: []});
      return device.serialNumber;
    };

    try {
      // Confirm that there are initially no paired devices.
      let result = await evalInWebView(webview, getUsbDevices, []);
      embedder.test.assertEq(0, result.length);
    } catch (ex) {
      embedder.test.fail();
    }

    try {
      // Attempting to pair from a webview should fail. This is expected to
      // throw.
      let result = await evalInWebView(webview, requestUsbDevice, []);
      embedder.test.fail();
    } catch (ex) {
    }

    try {
      // Confirm that there are still no paired devices.
      let result = await evalInWebView(webview, getUsbDevices, []);
      embedder.test.assertEq(0, result.length);
    } catch (ex) {
      embedder.test.fail();
    }

    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

// Before this test runs, the browser-side test code has a tab pair a USB device
// for the same origin used in the webview. We confirm that the webview cannot
// reuse this permission.
function testCannotReuseUsbPairedInTab() {
  let webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  webview.addEventListener('loadstop', async () => {
    let getUsbDevices = async () => {
      let devices = await navigator.usb.getDevices();
      return devices.map(device => device.serialNumber);
    };
    try {
      let result = await evalInWebView(webview, getUsbDevices, []);
      embedder.test.assertEq(0, result.length);
    } catch (ex) {
      embedder.test.fail();
    }

    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

function testCannotRequestFonts() {
  let webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  webview.addEventListener('loadstop', async () => {
    let getFonts = async () => {
      let fonts = await window.queryLocalFonts();
      return fonts.map(font => font.fullName);
    };

    try {
      let result = await evalInWebView(webview, getFonts, []);
      embedder.test.assertEq(0, result.length);
    } catch (ex) {
      embedder.test.fail();
    }
    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

// Before this test runs, the browser-side test code has a tab paired to a
// Serial port for the same origin used in the webview. We confirm that the
// webview cannot access or request the port.
function testSerialDisabled() {
  const webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  webview.addEventListener('loadstop', async () => {
    const getSerialPorts = async () => {
      const ports = await navigator.serial.getPorts();
      return ports;
    };

    const requestSerialPort = async () => {
      const port = await navigator.serial.requestPort();
      return port.getInfo;
    };

    try {
      // Confirm that no port is available for WebView.
      const result = await evalInWebView(webview, getSerialPorts, []);
      embedder.test.assertEq(0, result.length);
    } catch (_) {
      embedder.test.fail();
    }

    try {
      // Attempting to request a port should fail, expecting an exception.
      await evalInWebView(webview, requestSerialPort, []);
      // It's unexpected behavior for execution to end up here, so trigger a
      // test failure.
      embedder.test.fail();
    } catch (_) {
      // We expect an exception while requesting a port, so do nothing.
    }

    try {
      // Confirm that there is still no port available.
      const result = await evalInWebView(webview, getSerialPorts, []);
      embedder.test.assertEq(0, result.length);
    } catch (_) {
      embedder.test.fail();
    }

    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

// Before this test runs, the browser-side test code has a tab paired to a
// Bluetooth device for the same origin used in the webview. We confirm that the
// webview cannot request the device.
function testBluetoothDisabled() {
  const webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;
  webview.addEventListener('loadstop', async () => {
    const getBluetoothDeviceName = async () => {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{services: ['heart_rate']}]
      });
      return device.name;
    };

    try {
      const name = await evalInWebView(webview, getBluetoothDeviceName, []);
      // Expecting the bluetooth request to throw, therefore test would fail if
      // it reaches here.
      embedder.test.fail();
    } catch (e) {
    }

    embedder.test.succeed();
  });

  document.body.appendChild(webview);
}

function testCannotLockKeyboard() {
  const webview = document.createElement('webview');
  webview.src = embedder.emptyGuestURL;

  webview.addEventListener('loadstop', async () => {
    const lockKeyboard = () => {
      return navigator.keyboard.lock();
    };

    try {
      await evalInWebView(webview, lockKeyboard, []);
      // The attempt to lock the keyboard should fail.
      embedder.test.fail();
    } catch {
      embedder.test.succeed();
    }
  });

  document.body.appendChild(webview);
}

embedder.test.testList = {
  'testAllowTransparencyAttribute': testAllowTransparencyAttribute,
  'testAutosizeHeight': testAutosizeHeight,
  'testAutosizeAfterNavigation': testAutosizeAfterNavigation,
  'testAutosizeBeforeNavigation': testAutosizeBeforeNavigation,
  'testAutosizeRemoveAttributes': testAutosizeRemoveAttributes,
  'testAutosizeWithPartialAttributes': testAutosizeWithPartialAttributes,
  'testAPIMethodExistence': testAPIMethodExistence,
  'testBlankWebview': testBlankWebview,
  'testCustomElementCallbacksInaccessible':
      testCustomElementCallbacksInaccessible,
  'testChromeExtensionURL': testChromeExtensionURL,
  'testChromeExtensionRelativePath': testChromeExtensionRelativePath,
  'testDisplayNoneWebviewLoad': testDisplayNoneWebviewLoad,
  'testDisplayNoneWebviewRemoveChild': testDisplayNoneWebviewRemoveChild,
  'testInlineScriptFromAccessibleResources':
      testInlineScriptFromAccessibleResources,
  'testInvalidChromeExtensionURL': testInvalidChromeExtensionURL,
  'testWebRequestAPIExistence': testWebRequestAPIExistence,
  'testWebRequestAPIAddListener': testWebRequestAPIAddListener,
  'testEventName': testEventName,
  'testOnEventProperties': testOnEventProperties,
  'testLoadProgressEvent': testLoadProgressEvent,
  'testDestroyOnEventListener': testDestroyOnEventListener,
  'testCannotMutateEventName': testCannotMutateEventName,
  'testPartitionChangeAfterNavigation': testPartitionChangeAfterNavigation,
  'testPartitionRemovalAfterNavigationFails':
      testPartitionRemovalAfterNavigationFails,
  'testAddContentScript': testAddContentScript,
  'testAddMultipleContentScripts': testAddMultipleContentScripts,
  'testAddContentScriptWithSameNameShouldOverwriteTheExistingOne':
      testAddContentScriptWithSameNameShouldOverwriteTheExistingOne,
  'testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView':
      testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView,
  'testAddAndRemoveContentScripts': testAddAndRemoveContentScripts,
  'testAddContentScriptsWithNewWindowAPI':
      testAddContentScriptsWithNewWindowAPI,
  'testContentInitiatedNavigationToDataUrlBlocked':
      testContentInitiatedNavigationToDataUrlBlocked,
  'testContentScriptIsInjectedAfterTerminateAndReloadWebView':
      testContentScriptIsInjectedAfterTerminateAndReloadWebView,
  'testContentScriptExistsAsLongAsWebViewTagExists':
      testContentScriptExistsAsLongAsWebViewTagExists,
  'testAddContentScriptWithCode': testAddContentScriptWithCode,
  'testAddMultipleContentScriptsWithCodeAndCheckGeneratedScriptUrl':
      testAddMultipleContentScriptsWithCodeAndCheckGeneratedScriptUrl,
  'testExecuteScriptFail': testExecuteScriptFail,
  'testExecuteScript': testExecuteScript,
  'testExecuteScriptIsAbortedWhenWebViewSourceIsChanged':
      testExecuteScriptIsAbortedWhenWebViewSourceIsChanged,
  'testExecuteScriptIsAbortedWhenWebViewSourceIsInvalid':
      testExecuteScriptIsAbortedWhenWebViewSourceIsInvalid,
  'testTerminateAfterExit': testTerminateAfterExit,
  'testAssignSrcAfterCrash': testAssignSrcAfterCrash,
  'testNavOnConsecutiveSrcAttributeChanges':
      testNavOnConsecutiveSrcAttributeChanges,
  'testNavOnSrcAttributeChange': testNavOnSrcAttributeChange,
  'testNestedCrossOriginSubframes': testNestedCrossOriginSubframes,
  'testNestedSubframes': testNestedSubframes,
  'testReassignSrcAttribute': testReassignSrcAttribute,
  'testRemoveSrcAttribute': testRemoveSrcAttribute,
  'testPluginLoadPermission': testPluginLoadPermission,
  'testNewWindow': testNewWindow,
  'testNewWindowTwoListeners': testNewWindowTwoListeners,
  'testNewWindowNoPreventDefault': testNewWindowNoPreventDefault,
  'testNewWindowNoReferrerLink': testNewWindowNoReferrerLink,
  'testNewWindowAttachToExisting': testNewWindowAttachToExisting,
  'testContentLoadEvent': testContentLoadEvent,
  'testContentLoadEventWithDisplayNone': testContentLoadEventWithDisplayNone,
  'testDeclarativeWebRequestAPI': testDeclarativeWebRequestAPI,
  'testDeclarativeWebRequestAPISendMessage':
      testDeclarativeWebRequestAPISendMessage,
  'testDeclarativeWebRequestAPISendMessageSecondWebView':
      testDeclarativeWebRequestAPISendMessageSecondWebView,
  'testDisplayBlock': testDisplayBlock,
  'testWebRequestAPI': testWebRequestAPI,
  'testWebRequestAPIOnlyForInstance': testWebRequestAPIOnlyForInstance,
  'testWebRequestAPIErrorOccurred': testWebRequestAPIErrorOccurred,
  'testWebRequestAPIWithHeaders': testWebRequestAPIWithHeaders,
  'testWebRequestAPIGoogleProperty': testWebRequestAPIGoogleProperty,
  'testWebRequestListenerSurvivesReparenting':
      testWebRequestListenerSurvivesReparenting,
  'testGetProcessId': testGetProcessId,
  'testHiddenBeforeNavigation': testHiddenBeforeNavigation,
  'testLoadStartLoadRedirect': testLoadStartLoadRedirect,
  'testLoadAbortChromeExtensionURLWrongPartition':
      testLoadAbortChromeExtensionURLWrongPartition,
  'testLoadAbortEmptyResponse': testLoadAbortEmptyResponse,
  'testLoadAbortIllegalChromeURL': testLoadAbortIllegalChromeURL,
  'testLoadAbortIllegalFileURL': testLoadAbortIllegalFileURL,
  'testLoadAbortIllegalJavaScriptURL': testLoadAbortIllegalJavaScriptURL,
  'testLoadAbortInvalidNavigation': testLoadAbortInvalidNavigation,
  'testLoadAbortNonWebSafeScheme': testLoadAbortNonWebSafeScheme,
  'testLoadAbortSafeBrowsing': testLoadAbortSafeBrowsing,
  'testNavigateAfterResize': testNavigateAfterResize,
  'testNavigationToExternalProtocol': testNavigationToExternalProtocol,
  'testReload': testReload,
  'testReloadAfterTerminate': testReloadAfterTerminate,
  'testRemoveWebviewOnExit': testRemoveWebviewOnExit,
  'testRemoveWebviewAfterNavigation': testRemoveWebviewAfterNavigation,
  'testResizeWebviewResizesContent': testResizeWebviewResizesContent,
  'testResizeWebviewWithDisplayNoneResizesContent':
      testResizeWebviewWithDisplayNoneResizesContent,
  'testPostMessageCommChannel': testPostMessageCommChannel,
  'testScreenshotCapture': testScreenshotCapture,
  'testZoomAPI': testZoomAPI,
  'testFindAPI': testFindAPI,
  'testFindAPI_findupdate': testFindAPI_findupdate,
  'testFindInMultipleWebViews': testFindInMultipleWebViews,
  'testFindAfterTerminate': testFindAfterTerminate,
  'testLoadDataAPI': testLoadDataAPI,
  'testLoadDataAPIAccessibleResources': testLoadDataAPIAccessibleResources,
  'testResizeEvents': testResizeEvents,
  'testPerOriginZoomMode': testPerOriginZoomMode,
  'testPerViewZoomMode': testPerViewZoomMode,
  'testDisabledZoomMode': testDisabledZoomMode,
  'testZoomBeforeNavigation': testZoomBeforeNavigation,
  'testPlugin': testPlugin,
  'testGarbageCollect': testGarbageCollect,
  'testCloseNewWindowCleanup': testCloseNewWindowCleanup,
  'testFocusWhileFocused': testFocusWhileFocused,
  'testPDFInWebview': testPDFInWebview,
  'testNavigateToPDFInWebview': testNavigateToPDFInWebview,
  'testDialogInPdf': testDialogInPdf,
  'testMailtoLink': testMailtoLink,
  'testRendererNavigationRedirectWhileUnattached':
      testRendererNavigationRedirectWhileUnattached,
  'testRemoveBeforeAttach': testRemoveBeforeAttach,
  'testBlobURL': testBlobURL,
  'testWebViewAndEmbedderInNewWindow': testWebViewAndEmbedderInNewWindow,
  'testWebViewAndEmbedderInNewWindow_Noopener':
      testWebViewAndEmbedderInNewWindow_Noopener,
  'testNewWindowNoDeadlock': testNewWindowNoDeadlock,
  'testSelectPopupPositionInMac': testSelectPopupPositionInMac,
  'testWebRequestBlockedNavigation': testWebRequestBlockedNavigation,
  'testAddFencedFrame': testAddFencedFrame,
  'testInsertIntoIframe': testInsertIntoIframe,
  'testCreateAndInsertInIframe': testCreateAndInsertInIframe,
  'testInsertIntoMainFrameFromIframe': testInsertIntoMainFrameFromIframe,
  'testInsertIntoOtherWindow': testInsertIntoOtherWindow,
  'testCreateAndInsertInOtherWindow': testCreateAndInsertInOtherWindow,
  'testInsertFromOtherWindow': testInsertFromOtherWindow,
  'testInsertIntoDetachedIframe': testInsertIntoDetachedIframe,
  'testCannotRequestUsb': testCannotRequestUsb,
  'testCannotReuseUsbPairedInTab': testCannotReuseUsbPairedInTab,
  'testCannotRequestFonts': testCannotRequestFonts,
  'testSerialDisabled': testSerialDisabled,
  'testBluetoothDisabled': testBluetoothDisabled,
  'testCannotLockKeyboard': testCannotLockKeyboard,
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    chrome.test.sendMessage("Launched");
  });
};
