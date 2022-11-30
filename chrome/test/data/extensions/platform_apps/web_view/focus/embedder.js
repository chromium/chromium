// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_webview = null;
var embedder = {};
var seenFocusCount = 0;
embedder.tests = {};
embedder.guestURL =
    'data:text/html,<html><body>Guest<body></html>';
var g_inputMethodTestHelper = null;
var g_focusRestoredTestHelper = null;

window.runTest = function(testName) {
  if (!embedder.test.testList[testName]) {
    console.log('Incorrect testName: ' + testName);
    embedder.test.fail();
    return;
  }

  // Run the test.
  embedder.test.testList[testName]();
};

window.runCommand = function(command, opt_step) {
  window.console.log('window.runCommand: ' + command);
  switch (command) {
    case 'testFocusTracksEmbedderRunNextStep':
      testFocusTracksEmbedderRunNextStep();
      break;
    case 'testInputMethodRunNextStep':
      testInputMethodRunNextStep(opt_step);
      break;
    case 'testFocusRestoredRunNextStep':
      testFocusRestoredRunNextStep(opt_step);
      break;
    case 'testKeyboardFocusRunNextStep':
      testKeyboardFocusRunNextStep(opt_step);
      break;
    case 'monitorGuestEvent':
      monitorGuestEvent(opt_step);
    case 'waitGuestEvent':
      waitGuestEvent(opt_step);
    default:
      embedder.test.fail();
  }
};
// window.* exported functions end.

var LOG = function(msg) {
  window.console.log(msg);
};

embedder.test = {};
embedder.test.succeed = function() {
  chrome.test.sendMessage('TEST_PASSED');
};

embedder.test.fail = function() {
  chrome.test.sendMessage('TEST_FAILED');
};

embedder.test.assertEq = function(a, b) {
  if (a != b) {
    console.log('assertion failed: ' + a + ' != ' + b);
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

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    embedder.test.fail('No <webview> element created');
  }
  return webview;
};

/**
 * @private
 *
 * A test helper for focus related tests.
 * It does the following steps:
 * 1. It navigates |webview|.
 * 2. On 'loadstop', it injects the script |inject_js_guest_url|.
 * 3. When the injection has completed, it sends a ['connect'] message to the
 * guest to initiate a two-way communication channel.
 * 4. When the two way channel has been established |channelCreationCallback| is
 * called.
 * 5. It ignores all messages from the guest until it gets an
 * |expectedResponse|.
 * If there is no |expectedResponse|, the method is done.
 * 6. Once the expected result is received, call |responseCallback|.
 */
embedder.waitForResponseFromGuest_ =
    function(webview,
             inject_js_guest_url,
             channelCreationCallback,
             expectedResponse,
             responseCallback) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'connected') {
      channelCreationCallback(webview);

      if (!expectedResponse) {
        // We are done.
        window.removeEventListener('message', onPostMessageReceived);
      }
      return;
    }
    if (response != expectedResponse) {
      return;
    }
    responseCallback(data);
    window.removeEventListener('message', onPostMessageReceived);
  };
  window.addEventListener('message', onPostMessageReceived);

  webview.addEventListener('consolemessage', function(e) {
    LOG('g: ' + e.message);
  });

  var onWebViewLoadStop = function(e) {
    console.log('loadstop');
    webview.executeScript(
      {file: inject_js_guest_url},
      function(results) {
        console.log('Injected script into webview.');
        // Establish a communication channel with the webview1's guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
    webview.removeEventListener('loadstop', onWebViewLoadStop);
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  webview.src = embedder.guestURL;
};

// Helper class for testFocusRestored.
//
// This test has multiple steps, WebViewTest instructs this test to advance to
// each step and then performs some action and verification and runs the next
// step.
// See WebViewInteractiveTest.Focus_FocusRestored for details.
function FocusRestoredTestHelper() {
  // Total number of steps for this test that we run thru
  // testFocusRestoredRunNextStep.
  this.TOTAL_STEPS = 3;
  // Currently running step index.
  this.step_ = 0;
  this.messageHandlerRegistered_ = false;
  this.doneCallback_ = null;
}

FocusRestoredTestHelper.prototype.runStep = function(step, doneCallback) {
  LOG('runStep: ' + step);
  this.doneCallback_ = doneCallback;

  if (step != this.step_ + 1 || step < 0 || step > this.TOTAL_STEPS) {
    LOG('Incorrect step, expected:', this.step_ + 1, 'got', step);
    this.passStep_(false);
    return;
  }
  this.step_ = step;

  if (!this.messageHandlerRegistered_) {
    this.messageHandlerRegistered_ = true;
    window.addEventListener('message', this.messageHandler_.bind(this));
  }

  var msgToSend = '';
  if (step == 1) {
    msgToSend = 'request-waitForFocus';
  } else if (step == 2) {
    msgToSend = 'request-waitForBlur';
  } else if (step == 3) {
    msgToSend = 'request-waitForFocusAgain';
  }

  if (!msgToSend) {
    this.passStep_(false);
    return;
  }

  g_webview.contentWindow.postMessage(JSON.stringify([msgToSend]), '*');
};

FocusRestoredTestHelper.prototype.messageHandler_ = function(e) {
  var data = JSON.parse(e.data);
  LOG('FocusRestoredTestHelper.message, data: ' + data);
  switch (this.step_) {
    case 1:
      this.passStep_(data[0] == 'response-focus');
      break;
    case 2:
      this.passStep_(data[0] == 'response-blur');
      g_webview.focus();
      break;
    case 3:
      this.passStep_(data[0] == 'response-focusAgain');
      break;
    default:
      LOG('Unexpected message: ' + data);
      this.passStep_(false);
  }
};

FocusRestoredTestHelper.prototype.passStep_ = function(passed) {
  if (!this.doneCallback_) {
    LOG('Expected doneCallback_ in FocusRestoredTestHelper');
    embedder.test.fail();
    return;
  }
  this.doneCallback_(passed);
};

// Helper class for testInputMethod.
// This test has multiple steps, WebViewTest instructs this test to advance to
// each step and then performs some action and verification and runs the next
// step.
//
// See WebViewInteractiveTest.Focus_InputMethod for details about these steps.
function InputMethodTestHelper() {
  // Total number of steps for this test that we run thru
  // testInputMethodRunNextStep.
  this.TOTAL_STEPS = 3;
  // Currently running step index.
  this.step_ = 0;
  // True iff post message handler has been regsitered.
  this.messageHandlerRegistered_ = false;
  this.doneCallback_ = null;
};

InputMethodTestHelper.prototype.registerMessageHandler = function() {
  if (!this.messageHandlerRegistered_) {
    window.addEventListener('message', this.messageHandler_.bind(this));
    this.messageHandlerRegistered_ = true;
  }
};

InputMethodTestHelper.prototype.passStep_ = function(passed) {
  if (!this.doneCallback_) {
    LOG('Expected doneCallback_ in InputMethodTestHelper');
    embedder.test.fail();
    return;
  }
  this.doneCallback_(passed);
};

InputMethodTestHelper.prototype.runStep = function(step, doneCallback) {
  LOG('runStep: ' + step);
  this.doneCallback_ = doneCallback;

  if (step != this.step_ + 1 || step < 0 || step > this.TOTAL_STEPS) {
    LOG('Incorrect step, expected:', this.step_ + 1, 'got', step);
    this.passStep_(false);
    return;
  }
  this.step_ = step;

  var msgToSend = '';
  if (step == 1) {
    msgToSend = 'request-waitForOnInput';
  } else if (step == 2) {
    msgToSend = 'request-waitForOnInputAndSelect';
  } else if (step == 3) {
    msgToSend = 'request-valueAfterExtendSelection';
  }
  if (!msgToSend) {
    this.passStep_(false);
    return;
  }

  g_webview.contentWindow.postMessage(JSON.stringify([msgToSend]), '*');
};

InputMethodTestHelper.prototype.messageHandler_ = function(e) {
  var data = JSON.parse(e.data);
  LOG('InputMethodTestHelper.message, data: ' + data);

  if (data[0]=='response-seenFocus') {
    embedder.test.succeed();
    return;
  }

  switch (this.step_) {
    case 1:
      this.passStep_(data[0] == 'response-waitForOnInput' &&
                     data[1] == 'InputTest123');
      break;
    case 2:
      this.passStep_(data[0] == 'response-waitForOnInputAndSelect' &&
                     data[1] == 'InputTest456');
      break;
    case 3:
      this.passStep_(data[0] == 'response-valueAfterExtendSelection' &&
                     data[1] == 'Input456');
      break;
    default:
      LOG('Unexpected message: ' + data);
      this.passStep_(false);
  }
};

// Tests begin.

// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

embedder.testFocus_ = function(channelCreationCallback,
                               expectedResponse,
                               responseCallback) {
  var webview = embedder.setUpGuest_();

  embedder.waitForResponseFromGuest_(webview,
                                     'inject_focus.js',
                                     channelCreationCallback,
                                     expectedResponse,
                                     responseCallback);
};

// Verifies that if a <webview> is focused before navigation then the guest
// starts off focused.
//
// We create a <webview> element and make it focused before navigating it.
// Then we load a URL in it and make sure document.hasFocus() returns true
// for the <webview>.
function testFocusBeforeNavigation() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);

  var onChannelEstablished = function(webview) {
    // Query the guest if it has focus.
    var msg = ['request-hasFocus'];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  };

  // Focus the <webview> before navigating it.
  webview.focus();

  embedder.waitForResponseFromGuest_(
    webview,
    'inject_focus.js',
    onChannelEstablished,
    'response-hasFocus',
    function(data) {
      LOG('data, hasFocus: ' + data[1]);
      embedder.test.assertEq(true, data[1]);
      embedder.test.succeed();
    });
}

function testFocusEvent() {
  var seenResponse = false;
  embedder.testFocus_(function(webview) {
    webview.focus();
  }, 'focused', function() {
    // The focus event fires three times on first focus. We only care about
    // the first focus.
    if (seenResponse) {
      return;
    }
    seenResponse = true;
    embedder.test.succeed();
  });
}

function testBlurEvent() {
  var seenResponse = false;
  embedder.testFocus_(function(webview) {
    webview.focus();
    webview.blur();
  }, 'blurred', function() {
    if (seenResponse) {
      return;
    }
    seenResponse = true;
    embedder.test.succeed();
  });
}

// This test verifies that keyboard input is correctly routed into the guest.
//
// 1) Load the guest and attach an <input> to the guest dom. Count the number of
// input events sent to that element.
// 2) C++ simulates a mouse over and click of the <input> element and waits for
// the browser to see the guest main frame as focused.
// 3) Injects the key sequence: a, Shift+b, c.
// 4) In the second step, the test waits for the input events to be processed
// and then expects the vaue of the <input> to be what the test sent, notably:
// aBc.
function testKeyboardFocusImpl(input_length) {
  embedder.testFocus_(function(webview) {
    var created = function(e) {
      var data = JSON.parse(e.data);
      if (data[0] === 'response-createdInput') {
        chrome.test.sendMessage('TEST_PASSED');
        window.removeEventListener('message', created);
      }
    };
    window.addEventListener('message', created);

    g_webview = webview;
    var msg = ['request-createInput', input_length];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  }, 'response-elementClicked', function() {
        chrome.test.sendMessage('TEST_STEP_PASSED');
  });

}

function testKeyboardFocusRunNextStep(expected) {
  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    LOG('send window.message, data: ' + data);
    if (data[0] == 'response-inputValue') {
      if (data[1] == expected) {
        chrome.test.sendMessage('TEST_STEP_PASSED');
      } else {
        chrome.test.sendMessage('TEST_STEP_FAILED');
      }
    }
  });

  g_webview.contentWindow.postMessage(
      JSON.stringify(['request-getInputValue']), '*');
}

function testKeyboardFocusSimple() {
  testKeyboardFocusImpl(3);
}

function testKeyboardFocusWindowFocusCycle() {
  testKeyboardFocusImpl(6);
}

// This test verifies IME related stuff for guest.
//
// Briefly:
// 1) We load a guest, the guest gets initial focus and sends message
// back to the embedder.
// 2) In InputMethodTestHelper's step 1, we receive some text via cpp, the
// text is InputTest123, we verify we've seen the change in the guest.
// 3) In InputMethodTestHelper's step 2, we expect the text to be changed
// to InputTest456, this is done from cpp via committing an IME composition.
// 4) In InputMethodTestHelper's step 3, we have a composition (InputTest789)
// on an input element but we move the focus to another input element, we
// make sure the first element gets the composition commit.
// 5) In InputMethodTestHelper's step 4, we verify extending and deleting
// selection through caret works properly.
function testInputMethod() {
  var webview = document.createElement('webview');
  g_webview = webview;
  document.body.appendChild(webview);

  var onChannelEstablished = function(webview) {};

  if (!g_inputMethodTestHelper) {
    g_inputMethodTestHelper = new InputMethodTestHelper();
  }

  embedder.waitForResponseFromGuest_(
      webview,
      'inject_input_method.js',
      onChannelEstablished,
      'response-inputMethodPreparedForFocus',
      function(data) {
        g_inputMethodTestHelper.registerMessageHandler();
        webview.focus();
        webview.contentWindow.postMessage(
            JSON.stringify(['request-waitForFocus']), '*');
      });
}

// Runs additional test steps for testInputMethod.
function testInputMethodRunNextStep(step) {
  LOG('testInputMethodRunNextStep, step: ' + step);
  if (!g_inputMethodTestHelper) {
    g_inputMethodTestHelper = new InputMethodTestHelper();
  }

  g_inputMethodTestHelper.runStep(step, function(stepPassed) {
    LOG('runStep callback, stepPassed: ' + stepPassed);
    chrome.test.sendMessage(stepPassed ? 'TEST_STEP_PASSED'
                                       : 'TEST_STEP_FAILED');
  });
}

// This test ensures we get TextInputTypeChanged event if we bring
// back focus to a guest's <input> after it was initially focused.
//
// Briefly:
// 1) We load a guest.
// 2) In FocusRestoredTestHelper's step 1, we click on the guest to
// focus its <input> element.
// 3) In FocusRestoredTestHelper's step 2, we click outside the guest
// so that the <input> gets a blur event.
// 4. In FocusRestoredTestHelper's step 3, we click on the guest again
// to bring the focus back.
// In the end we check the guest rvh's TextInputType in cpp to make
// sure it initialises properly.
function testFocusRestored() {
  var webview = document.createElement('webview');
  webview.style.width = '100px';
  webview.style.height = '100px';
  g_webview = webview;
  document.body.appendChild(webview);

  webview.focus();

  var onChannelEstablished = function(webview) {
    chrome.test.sendMessage('TEST_PASSED');
  };

  embedder.waitForResponseFromGuest_(webview,
                                     'inject_focus_restored.js',
                                     onChannelEstablished,
                                     undefined,
                                     undefined);
}

// Runs additional test steps for testFocusRestored.
function testFocusRestoredRunNextStep(step) {
  LOG('testFocusRestoredRunNextStep, step: ' + step);
  if (!g_focusRestoredTestHelper) {
    g_focusRestoredTestHelper = new FocusRestoredTestHelper();
  }
  g_focusRestoredTestHelper.runStep(step, function(stepPassed) {
    LOG('runStep callback, stepPassed: ' + stepPassed);
    chrome.test.sendMessage(stepPassed ? 'TEST_STEP_PASSED'
                                       : 'TEST_STEP_FAILED');
  });
}

// Ensures that the tab key can be used to navigate out of the webview. There is
// a corner case where focus can be trapped in the webview if the next focusable
// element in the embedder is focused when trying to tab or the previous element
// when using shift-tab.
//
// Briefly:
// 1) Start with the embedder input focused
// 2) Click the guest input and wait for it to be focused
// 3) Send a tab key event
// 4) Wait for the embedder input to receive another input event.
function testFocusTakeFocus() {
  var input = document.createElement('input');
  var webview = embedder.setUpGuest_();
  g_webview = webview;
  document.body.appendChild(input);

  var onChannelEstablished = function(webview) {
    input.focus();

    var msg = ['request-coords'];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  };

  var inputFocusedHandler = function(e) {
      chrome.test.sendMessage('TEST_STEP_PASSED');
  }

  var guestFocusedHandler = function(e) {
      console.log('input focused in guest');
      window.removeEventListener('message', guestFocusedHandler);
      input.addEventListener('focus', inputFocusedHandler);
      chrome.test.sendMessage('TEST_STEP_PASSED');
  };

  var coordHandler = function(response) {
    var rect = g_webview.getBoundingClientRect();
    window.clickX = rect.left + response[1];
    window.clickY = rect.top + response[2];
    window.addEventListener('message', guestFocusedHandler);
    chrome.test.sendMessage('TEST_PASSED');
  };

  embedder.waitForResponseFromGuest_(webview,
      'inject_focus_take_focus.js',
      onChannelEstablished,
      'response-coords',
      coordHandler);
}

// Tests that if we focus/blur the embedder, it also gets reflected in the
// guest.
//
// This test has two steps:
// 1) testFocusTracksEmbedder(), in this step we create a <webview> and
// focus it before navigating. After navigating it to a URL, we focus an input
// element inside the <webview>, and wait for its 'focus' event to fire.
// 2) testFocusTracksEmbedderRunNextStep(), in this step, we have already called
// Blur() on the embedder's RVH (see WebViewTest.Focus_FocusTracksEmbedder),
// we make sure we see a 'blur' event on the <webview>'s input element.
function testFocusTracksEmbedder() {
  var webview = document.createElement('webview');
  g_webview = webview;
  document.body.appendChild(webview);

  var onChannelEstablished = function(webview) {
    var msg = ['request-waitForFocus'];
    webview.contentWindow.postMessage(JSON.stringify(msg), '*');
  };

  // Focus the <webview> before navigating it.
  // This is necessary so that 'blur' event on guest's <input> element fires.
  webview.focus();

  embedder.waitForResponseFromGuest_(
      webview,
      'inject_focus.js',
      onChannelEstablished,
      'response-seenFocus',
      function(data) { embedder.test.succeed(); });
}

// Runs the second step for testFocusTracksEmbedder().
// See WebViewTest.Focus_FocusTracksEmbedder() to see how this is invoked.
function testFocusTracksEmbedderRunNextStep() {
  g_webview.contentWindow.postMessage(
      JSON.stringify(['request-waitForBlurAfterFocus']), '*');

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    LOG('send window.message, data: ' + data);
    if (data[0] == 'response-seenBlurAfterFocus')
      chrome.test.sendMessage('TEST_STEP_PASSED');
  });
}

// Tests that <webview> sees advanceFocus() call when we cycle through the
// elements inside it using tab key.
//
// This test has two steps:
// 1) testAdvanceFocus(), in this step, we focus the embedder and press a
// tab key, we expect the input element inside the <webview> to be focused.
// 2) POST_testAdvanceFocus(), in this step we send additional tab keypress
// to the embedder/app (from WebViewInteractiveTest.Focus_AdvanceFocus), this
// would cycle the focus within the elements and will bring focus back to
// the input element present in the <webview> mentioned in step 1.
function testAdvanceFocus() {
  var webview = document.createElement('webview');
  g_webview = webview;
  document.body.appendChild(webview);

  webview.addEventListener('consolemessage', function(e) {
    LOG('g: ' + e.message);
  });
  webview.addEventListener('loadstop', function(e) {
    LOG('loadstop');

    window.addEventListener('message', function(e) {
      var data = JSON.parse(e.data);
      LOG('message, data: ' + data);

      if (data[0] == 'connected') {
        embedder.test.succeed();
      } else if (data[0] == 'button1-focused') {
        var focusCount = data[1];
        LOG('focusCount: ' + focusCount);
        seenFocusCount++;
        if (focusCount == 1) {
          chrome.test.sendMessage('button1-focused');
        } else {
          chrome.test.sendMessage('button1-advance-focus');
        }
      }
    });

    webview.executeScript(
      {file: 'inject_advance_focus_test.js'},
      function(results) {
        window.console.log('webview.executeScript response');
        if (!results || !results.length) {
          LOG('Inject script failure.');
          embedder.test.fail();
          return;
        }
        webview.contentWindow.postMessage(JSON.stringify(['connect']), '*');
      });
  });

  webview.src = embedder.guestURL;
}

function monitorGuestEvent(type) {
  g_webview.contentWindow.postMessage(
      JSON.stringify(['request-monitorEvent', type]), '*');
}

function waitGuestEvent(type) {
  var listener = function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'response-waitEvent') {
      window.removeEventListener('message', listener);
      if (data[1] == type) {
        chrome.test.sendMessage('TEST_STEP_PASSED');
      } else {
        chrome.test.sendMessage('TEST_STEP_FAILED');
      }
    }
  }
  window.addEventListener('message', listener);

  g_webview.contentWindow.postMessage(
      JSON.stringify(['request-waitEvent', type]), '*');
}

embedder.test.testList = {
  'testAdvanceFocus': testAdvanceFocus,
  'testBlurEvent': testBlurEvent,
  'testFocusBeforeNavigation': testFocusBeforeNavigation,
  'testFocusEvent': testFocusEvent,
  'testFocusTracksEmbedder': testFocusTracksEmbedder,
  'testInputMethod': testInputMethod,
  'testKeyboardFocusSimple': testKeyboardFocusSimple,
  'testKeyboardFocusWindowFocusCycle': testKeyboardFocusWindowFocusCycle,
  'testFocusRestored': testFocusRestored,
  'testFocusTakeFocus': testFocusTakeFocus
};

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.test.sendMessage('Launched');
  });
};
