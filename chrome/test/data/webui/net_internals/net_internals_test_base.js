// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends testing.Test
 */
function NetInternalsTest() {}

NetInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Define the C++ fixture class and include it.
   * @type {?string}
   * @override
   */
  typedefCppFixture: 'NetInternalsTest',

  /** @inheritDoc */
  browsePreload: 'chrome://net-internals/',

  /** @inheritDoc */
  isAsync: true,

  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    NetInternalsTest.activeTest = this;
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);
    window.setTimeout(runTest, 0);
  },
};

NetInternalsTest.activeTest = null;

/**
 * A callback function for use by asynchronous Tasks that need a return value
 * from the NetInternalsTest::MessageHandler.  Must be null when no such
 * Task is running.  Set by NetInternalsTest.setCallback.  Automatically
 * cleared once called.  Must only be called through
 * NetInternalsTest::MessageHandler::RunCallback, from the browser process.
 */
NetInternalsTest.callback = null;

/**
 * Sets NetInternalsTest.callback.  Any arguments will be passed to the
 * callback function.
 * @param {function} callbackFunction Callback function to be called from the
 *     browser.
 */
NetInternalsTest.setCallback = function(callbackFunction) {
  // Make sure no Task has already set the callback function.
  assertEquals(null, NetInternalsTest.callback);

  // Wrap |callbackFunction| in a function that clears
  // |NetInternalsTest.callback| before calling |callbackFunction|.
  var callbackFunctionWrapper = function() {
    NetInternalsTest.callback = null;
    callbackFunction.apply(null, Array.prototype.slice.call(arguments));
  };
  NetInternalsTest.callback = callbackFunctionWrapper;
};
