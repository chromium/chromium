/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Set to true while debugging virtual keyboard tests, for verbose debug output.
 */
var debugging = false;

/**
 * The enumeration of keyset modifiers.
 * @enum {string}
 */
var KeysetModifier =
    {NONE: 'none', SHIFT: 'shift', MORE: 'more', SYMBOL: 'symbol'};

/**
 * Flag values for the shift, control and alt modifiers as defined by
 * EventFlags in "event_constants.h".
 * @type {enum}
 */
var Modifier = {NONE: 0, SHIFT: 2, CONTROL: 4, ALT: 8};

/**
 * Display diagnostic messages when debugging tests.
 * @param {string} Message to conditionally display.
 */
function Debug(message) {
  if (debugging)
    console.log(message);
}

var mockController;

/**
 * Adds mocks for chrome extension API calls.
 *
 * @param {MockController} mockController Controller for validating
 *     calls with expectations.
 */
function mockExtensionApis(mockController) {
  /**
   * Mocks methods within a namespace.
   * @param {string} namespace Dot delimited namespace.
   * @param {Array<string>} methods List of methods names to mock.
   */
  var addMocks = function(namespace, methods) {
    var parts = namespace.split('.');
    var base = window;
    for (var i = 0; i < parts.length; i++) {
      if (!base[parts[i]])
        base[parts[i]] = {};
      base = base[parts[i]];
    }
    methods.forEach(function(m) {
      var fn = base[m] = mockController.createFunctionMock(m);
      fn.functionName = [namespace, m].join('.');
      // Method to arm triggering a callback function with the specified
      // arguments. Skip validation of callbacks.
      fn.setCallbackData = function() {
        fn.callbackData = Array.prototype.slice.call(arguments);
        fn.verifyMock = function() {};
      };
      // TODO(kevers): Add validateCall override that strips functions from the
      // argument signature before calling MockMethod.Prototype.validateCall
    });
  };

  var virtualKeyboardPrivateMethods = [
    'getKeyboardConfig', 'hideKeyboard', 'insertText', 'lockKeyboard',
    'moveCursor', 'sendKeyEvent', 'setMode', 'setKeyboardState',
    'setOccludedBounds', 'setHotrodKeyboard'
  ];

  var inputMethodPrivateMethods =
      ['getCurrentInputMethod', 'getInputMethods', 'setCurrentInputMethod'];

  addMocks('chrome.virtualKeyboardPrivate', virtualKeyboardPrivateMethods);
  addMocks('chrome.inputMethodPrivate', inputMethodPrivateMethods);
  addMocks('chrome.runtime', ['getBackgroundPage']);
  addMocks('chrome.runtime.onMessage', ['addListener']);
  // Ignore calls to addListener. Reevaluate if important to properly track the
  // flow of events.
  chrome.runtime.onMessage.addListener = function() {};

  chrome.i18n = {};
  chrome.i18n.getMessage = function(name) {
    return name;
  };
}

/**
 * Adjust the size and position of the keyboard for testing.
 */
function adjustScreenForTesting() {
  var style = document.body.style;
  style.left = 0;
  style.top = 0;
  style.bottom = 0;
  style.right = 0;
  style.background = 'transparent';
  style.position = 'absolute';

  // Adjust positioning for testing in a non-fullscreen display.
  Object.defineProperty(window.screen, 'width', {
    get: function() {
      return document.body.clientWidth;
    },
    configurable: true
  });
  Object.defineProperty(window.screen, 'height', {
    get: function() {
      return document.body.clientHeight;
    },
    configurable: true
  });
}

/**
 * Create mocks for the virtualKeyboardPrivate API. Any tests that trigger API
 * calls must set expectations for call signatures.
 */
function setUp() {
  mockController = new MockController();

  // It is not safe to install the mockTimer during setUp, as intialization of
  // the keyboard uses polling to determine when loading is complete. Instead,
  // install the mockTimer as needed once a test is initiated and unintall on
  // completion of the test.

  mockExtensionApis(mockController);

  adjustScreenForTesting();

  var validateSendCall = function(index, expected, observed) {
    // Only consider the first argument (VirtualKeyEvent) for the validation of
    // sendKeyEvent calls.
    var expectedEvent = expected[0];
    var observedEvent = observed[0];
    assertEquals(
        expectedEvent.type, observedEvent.type, 'Mismatched event types.');
    assertEquals(
        expectedEvent.charValue, observedEvent.charValue,
        'Mismatched unicode values for character.');
    assertEquals(
        expectedEvent.keyCode, observedEvent.keyCode, 'Mismatched key codes.');
    assertEquals(
        expectedEvent.modifiers, observedEvent.modifiers,
        'Mismatched states for modifiers.');
  };
  chrome.virtualKeyboardPrivate.sendKeyEvent.validateCall = validateSendCall;

  var validateLockKeyboard = function(index, expected, observed) {
    assertEquals(
        expected[0], observed[0], 'Mismatched keyboard lock/unlock state.');
  };
  chrome.virtualKeyboardPrivate.lockKeyboard.validateCall =
      validateLockKeyboard;

  chrome.virtualKeyboardPrivate.hideKeyboard.validateCall = function() {
    // hideKeyboard has one optional argument for error logging that does not
    // matter for the purpose of validating the call.
  };

  // Set data to be provided to callbacks in response to API calls.
  // TODO(kevers): Provide mechanism to override these values for individual
  // tests as needed.
  chrome.virtualKeyboardPrivate.getKeyboardConfig.setCallbackData({
    layout: 'qwerty',
    a11ymode: false,
    experimental: false,
    features: [],
  });

  chrome.inputMethodPrivate.getCurrentInputMethod.setCallbackData('us:en');

  // Set an empty list. Tests that care about input methods in the menu will
  // need to call this again with their own list of input methods.
  chrome.inputMethodPrivate.getInputMethods.setCallbackData([]);

  chrome.runtime.getBackgroundPage.setCallbackData(undefined);

  // TODO(kevers): Mock additional extension API calls as required.
}

/**
 * Verify that API calls match expectations.
 */
function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
}


// ------------------- Helper Functions -------------------------

/**
 * Runs a test asynchronously once keyboard loading is complete.
 * @param {Function} runTestCallback Asynchronous test function.
 * @param {Object=} opt_config Optional configuration for the keyboard.
 */
function onKeyboardReady(runTestCallback, opt_config) {
  var default_config = {
    keyset: 'us.compact.qwerty',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English'
  };
  var config = opt_config || default_config;
  var options = config.options || {};
  chrome.virtualKeyboardPrivate.keyboardLoaded = function() {
    runTestCallback();
  };
  window.initializeVirtualKeyboard(
      config.keyset, config.languageCode, config.passwordLayout, config.name,
      options);
}

/**
 * Defers continuation of a test until one or more keysets are loaded.
 * @param {string|Array<string>} keyset Name of the target keyset or list of
 *     keysets.
 * @param {Function} continueTestCallback Callback function to invoke in order
 *     to resume the test.
 */
function onKeysetsReady(keyset, continueTestCallback) {
  if (keyset instanceof Array) {
    var blocked = false;
    keyset.forEach(function(id) {
      if (!(id in controller.container_.keysetViewMap))
        blocked = true;
    });
    if (!blocked) {
      continueTestCallback();
      return;
    }
  } else if (keyset in controller.container_.keysetViewMap) {
    continueTestCallback();
    return;
  }
  setTimeout(function() {
    onKeysetsReady(keyset, continueTestCallback);
  }, 100);
}

/**
 * Mocks a touch event targeted on a key.
 * @param {!Element} key .
 * @param {string} eventType .
 */
function mockTouchEvent(key, eventType) {
  var rect = key.getBoundingClientRect();
  var x = rect.left + rect.width / 2;
  var y = rect.top + rect.height / 2;
  var e = document.createEvent('UIEvent');
  e.initUIEvent(eventType, true, true);
  e.touches = [{pageX: x, pageY: y}];
  e.target = key;
  return key.dispatchEvent(e);
}

/**
 * Simulates tapping on a key.
 * @param {!Element} key .
 */
function mockTap(key) {
  mockTouchEvent(key, 'touchstart');
  mockTouchEvent(key, 'touchend');
}

/**
 * Returns the active keyboard view.
 * @return {!HTMLElement}
 */
function getActiveView() {
  var container = document.querySelector('.inputview-container');
  var views = container.querySelectorAll('.inputview-view');
  for (var i = 0; i < views.length; i++) {
    var display = views[i].style.display;
    if (!display || display != 'none') {
      return views[i];
    }
  }
}

/**
 * Finds the ancestor element corresponding the the soft key view.
 * @param {Element} source .
 * @return {?Element} .
 */
function getSoftKeyView(source) {
  var parent = source.parentElement;
  while (parent && !parent.classList.contains('inputview-skv')) {
    parent = parent.parentElement;
  }
  return parent;
}

/**
 * Locates a key by label.
 * @param {string} label The label on the key.  If the key has multiple labels,
 *    |label| can match any of them.
 * @param {string=} opt_rowId Optional ID of the row containing the key.
 * @returns {?Element} .
 */
function findKey(label, opt_rowId) {
  var view = getActiveView();
  assertTrue(!!view, 'Unable to find active keyboard view');
  if (opt_rowId) {
    view = view.querySelector('#' + opt_rowId);
    assertTrue(!!view, 'Unable to locate row ' + opt_rowId);
  }
  var candidates = view.querySelectorAll('.inputview-ch');
  // Compact layouts use a different naming convention.
  if (candidates.length == 0)
    candidates = view.querySelectorAll('.inputview-special-key-name');
  for (var i = 0; i < candidates.length; i++) {
    if (candidates[i].textContent == label)
      return candidates[i];
  }
  assertTrue(false, 'Cannot find key labeled \'' + label + '\'');
}

/**
 * Locates a key with matching ID. Note that IDs are not necessarily unique
 * across keysets; however, it is unique within the active layout.
 */
function findKeyById(label) {
  var view = getActiveView();
  assertTrue(!!view, 'Unable to find active keyboard view');
  var key = view.querySelector('#' + label);
  assertTrue(!!key, 'Cannot find key with ID ' + label);
  return key;
}

/**
 * Verifies if a key contains a matching label.
 * @param {Element} key .
 * @param {string} label .
 * @return {boolean} .
 */
function hasLabel(key, label) {
  var characters = key.querySelectorAll('.inputview-ch');
  // Compact layouts represent keys differently.
  if (characters.length == 0)
    characters = key.querySelectorAll('.inputview-special-key-name');
  for (var i = 0; i < characters.length; i++) {
    if (characters[i].textContent == label)
      return true;
  }
  return false;
}

/**
 * Mock typing of basic keys. Each keystroke should trigger a pair of
 * API calls to send viritual key events.
 * @param {string} label The character being typed.
 * @param {number} keyCode The legacy key code for the character.
 * @param {number} modifiers Indicates which if any of the shift, control and
 *     alt keys are being virtually pressed.
 * @param {number=} opt_unicode Optional unicode value for the character. Only
 *     required if it cannot be directly calculated from the label.
 */
function mockTypeCharacter(label, keyCode, modifiers, opt_unicode) {
  var key = label.length == 1 ? findKey(label) : findKeyById(label);
  // opt_unicode is allowed to be 0.
  var unicodeValue = opt_unicode;
  if (unicodeValue === undefined)
    unicodeValue = label.charCodeAt(0);
  var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
  send.addExpectation({
    type: 'keydown',
    charValue: unicodeValue,
    keyCode: keyCode,
    modifiers: modifiers
  });
  send.addExpectation({
    type: 'keyup',
    charValue: unicodeValue,
    keyCode: keyCode,
    modifiers: modifiers
  });
  mockTap(key);
}

/**
 * Emulates a user triggering a keyset modifier.
 * @param {!KeysetModifier} The modifier to apply.
 */
function applyKeysetModifier(modifier) {
  if (modifier == KeysetModifier.NONE) {
    return;
  }
  var activeView = getActiveView();
  if (modifier == KeysetModifier.SHIFT) {
    // Set state of shift key.
    var leftShift = activeView.querySelector('#ShiftLeft');
    assertTrue(!!leftShift, 'Unable to find left shift key');
    var currentShiftState =
        !!leftShift.querySelector('.inputview-special-key-highlight');
    if (!currentShiftState) {
      mockTap(leftShift);
    }
  } else if (modifier == KeysetModifier.SYMBOL) {
    var switchKey = findKey('?123', 'spaceKeyrow');
    assertTrue(!!switchKey, 'Unable to find symbol transition key');
    // Switch to symbol keyset.
    mockTap(switchKey);
  } else {
    var switchKey = findKey('~[<', 'row3');
    assertTrue(!!switchKey, 'Unable to find more transition key');
    // Switch to more keyset.
    mockTap(switchKey);
  }
}
