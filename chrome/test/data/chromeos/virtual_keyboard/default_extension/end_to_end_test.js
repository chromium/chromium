/*
 * Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Defers continuation of a test until a keyset is loaded.
 * @param {string} keyset Name of the target keyset.
 * @param {Function} continueTestCallback Callback function to invoke in order
 *     to resume the test.
 */
function onKeysetReady(keyset, continueTestCallback) {
  var container = document.querySelector('.inputview-container');
  if (container) {
    var bounds = container.getBoundingClientRect();
    console.log(['container', container, bounds]);
    if (bounds.bottom > 0 && keyset in controller.container_.keysetViewMap &&
        keyset == controller.currentKeyset_) {
      continueTestCallback();
      return;
    }
  }
  setTimeout(function() {
    onKeysetReady(keyset, continueTestCallback);
  }, 100);
}


/**
 * Display an error message and abort the test.
 * @param {string} message The error message.
 */
function fail(message) {
  console.error(message);
  window.domAutomationController.send(false);
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
  key.dispatchEvent(e);
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
    if (!display || display != 'none')
      return views[i];
  }
  fail('Unable to find active keyboard view');
}


/**
 * Locates a key by label.
 * @param {string} label The label on the key. If the key has multiple labels,
 *    |label| can match any of them.
 * @returns {?Element} .
 */
function findKey(label) {
  var view = getActiveView();
  candidates = view.querySelectorAll('.inputview-special-key-name');
  for (var i = 0; i < candidates.length; i++) {
    if (candidates[i].textContent == label)
      return candidates[i];
  }
  fail('Cannot find key labeled \'' + label + '\'');
}


// Wait for keyboard to finish loading asynchronously before tapping key.
onKeysetReady('us.compact.qwerty', function() {
  mockTap(findKey('a'));
});
