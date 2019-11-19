// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var defaultFuzzFactor = 1;

function assertFuzzyEq(expected, actual, fuzzFactor, message) {
  if (!message) {
    message = "Expected: " + expected + "; actual: " + actual + "; "
               + "fuzzyFactor: " + fuzzFactor;
  }

  chrome.test.assertTrue(actual - fuzzFactor <= expected
                         && actual + fuzzFactor >= expected, message);

  if (actual != expected) {
    console.log("FUZZ: a factor of " + Math.abs(actual - expected) +
                "has been used.");
  }
}

// This helper will verify that |check| returns true. If it does not, it will do
// a trip to the event loop and will try again until |check| returns true. At
// which points |callback| will be called.
// NOTE: if the test fails, it will timeout.
function eventLoopCheck(check, callback) {
  if (check()) {
    callback();
  } else {
    setTimeout(callbackPass(function() { eventLoopCheck(check, callback); }));
  }
}

// This help function will call the callback when the window passed to it will
// be loaded. The callback will have the AppWindow passed as a parameter.
function waitForLoad(win, callback) {
  var window = win.contentWindow;

  if (window.document.readyState == 'complete') {
    callback(win);
    return;
  }

  window.addEventListener('load', callbackPass(function() {
    window.removeEventListener('load', arguments.callee);
    callback(win);
  }));
}

function assertConstraintsUnspecified(win) {
  chrome.test.assertEq(null, win.innerBounds.minWidth);
  chrome.test.assertEq(null, win.innerBounds.minHeight);
  chrome.test.assertEq(null, win.innerBounds.maxWidth);
  chrome.test.assertEq(null, win.innerBounds.maxHeight);
  chrome.test.assertEq(null, win.outerBounds.minWidth);
  chrome.test.assertEq(null, win.outerBounds.minHeight);
  chrome.test.assertEq(null, win.outerBounds.maxWidth);
  chrome.test.assertEq(null, win.outerBounds.maxHeight);
}

function assertBoundsConsistent(win) {
  // Ensure that the inner and outer bounds are consistent. Since platforms
  // have different frame padding, we cannot check the sizes precisely.
  // It is a reasonable assumption that all platforms will have a title bar at
  // the top of the window.
  chrome.test.assertTrue(win.innerBounds.left >= win.outerBounds.left);
  chrome.test.assertTrue(win.innerBounds.top > win.outerBounds.top);
  chrome.test.assertTrue(win.innerBounds.width <= win.outerBounds.width);
  chrome.test.assertTrue(win.innerBounds.height < win.outerBounds.height);

  if (win.innerBounds.minWidth === null)
    chrome.test.assertEq(null, win.outerBounds.minWidth);
  else
    chrome.test.assertTrue(
        win.innerBounds.minWidth <= win.outerBounds.minWidth);

  if (win.innerBounds.minHeight === null)
    chrome.test.assertEq(null, win.outerBounds.minHeight);
  else
    chrome.test.assertTrue(
        win.innerBounds.minHeight < win.outerBounds.minHeight);

  if (win.innerBounds.maxWidth === null)
    chrome.test.assertEq(null, win.outerBounds.maxWidth);
  else
    chrome.test.assertTrue(
        win.innerBounds.maxWidth <= win.outerBounds.maxWidth);

  if (win.innerBounds.maxHeight === null)
    chrome.test.assertEq(null, win.outerBounds.maxHeight);
  else
    chrome.test.assertTrue(
        win.innerBounds.maxHeight < win.outerBounds.maxHeight);
}

function testConflictingBoundsProperty(propertyName) {
  var innerBounds = {};
  var outerBounds = {};
  innerBounds[propertyName] = 20;
  outerBounds[propertyName] = 20;
  chrome.app.window.create('test.html', {
    innerBounds: innerBounds,
    outerBounds: outerBounds
  }, callbackFail('The ' + propertyName + ' property cannot be specified for ' +
                  'both inner and outer bounds.')
  );
}

function assertBoundsEq(expectedBounds, actualBounds) {
  chrome.test.assertEq(expectedBounds.left, actualBounds.left);
  chrome.test.assertEq(expectedBounds.top, actualBounds.top);
  chrome.test.assertEq(expectedBounds.width, actualBounds.width);
  chrome.test.assertEq(expectedBounds.height, actualBounds.height);
}

function assertConstraintsEq(expectedConstraints, actualConstraints) {
  chrome.test.assertEq(expectedConstraints.minWidth,
                       actualConstraints.minWidth);
  chrome.test.assertEq(expectedConstraints.minHeight,
                       actualConstraints.minHeight);
  chrome.test.assertEq(expectedConstraints.maxWidth,
                       actualConstraints.maxWidth);
  chrome.test.assertEq(expectedConstraints.maxHeight,
                       actualConstraints.maxHeight);
}

function runSetBoundsTest(boundsType, initialState, changeFields,
                          expectedBounds, hasConstraints) {
  var createOptions = {};
  createOptions[boundsType] = initialState;
  chrome.app.window.create('test.html', createOptions, callbackPass(
  function(win) {
    // Change the bounds.
    if (typeof(changeFields.left) !== 'undefined' &&
        typeof(changeFields.top) !== 'undefined') {
      win[boundsType].setPosition(changeFields.left, changeFields.top);
    } else if (typeof(changeFields.left) !== 'undefined')
      win[boundsType].left = changeFields.left;
    else if (typeof(changeFields.top) !== 'undefined')
      win[boundsType].top = changeFields.top;

    if (typeof(changeFields.width) !== 'undefined' &&
        typeof(changeFields.height) !== 'undefined') {
      win[boundsType].setSize(changeFields.width, changeFields.height);
    } else if (typeof(changeFields.width) !== 'undefined')
      win[boundsType].width = changeFields.width;
    else if (typeof(changeFields.height) !== 'undefined')
      win[boundsType].height = changeFields.height;

    // Dummy call to wait for bounds to be changed in the browser.
    chrome.test.waitForRoundTrip('msg', callbackPass(function(msg) {
      assertBoundsConsistent(win);
      assertBoundsEq(expectedBounds, win[boundsType]);
      if (!hasConstraints)
        assertConstraintsUnspecified(win);
      win.close();
    }));
  }));
}

function runSetConstraintsTest(boundsType, initialState, changeFields,
                               expectedConstraints, expectedBounds) {
  var createOptions = {};
  createOptions[boundsType] = initialState;
  chrome.app.window.create('test.html', createOptions, callbackPass(
  function(win) {
    assertConstraintsEq(initialState, win[boundsType]);

    // Change the constraints.
    if (typeof(changeFields.minWidth) !== 'undefined' &&
        typeof(changeFields.minHeight) !== 'undefined') {
      win[boundsType].setMinimumSize(changeFields.minWidth,
                                     changeFields.minHeight);
    } else if (typeof(changeFields.minWidth) !== 'undefined')
      win[boundsType].minWidth = changeFields.minWidth;
    else if (typeof(changeFields.minHeight) !== 'undefined')
      win[boundsType].minHeight = changeFields.minHeight;

    if (typeof(changeFields.maxWidth) !== 'undefined' &&
        typeof(changeFields.maxHeight) !== 'undefined') {
      win[boundsType].setMaximumSize(changeFields.maxWidth,
                                     changeFields.maxHeight);
    } else if (typeof(changeFields.maxWidth) !== 'undefined')
      win[boundsType].maxWidth = changeFields.maxWidth;
    else if (typeof(changeFields.maxHeight) !== 'undefined')
      win[boundsType].maxHeight = changeFields.maxHeight;

    // Dummy call to wait for the constraints to be changed in the browser.
    chrome.test.waitForRoundTrip('msg', callbackPass(function(msg) {
      assertBoundsConsistent(win);
      assertConstraintsEq(expectedConstraints, win[boundsType]);
      if (expectedBounds) {
        chrome.test.assertEq(expectedBounds.width, win[boundsType].width);
        chrome.test.assertEq(expectedBounds.height, win[boundsType].height);
      }
      win.close();
    }));
  }));
}

function testCreate() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html',
                               {id: 'testId'},
                               callbackPass(function(win) {
        chrome.test.assertEq(typeof win.contentWindow.window, 'object');
        chrome.test.assertTrue(
          typeof win.contentWindow.document !== 'undefined');
        chrome.test.assertFalse(
          'about:blank' === win.contentWindow.location.href);
        var cw = win.contentWindow.chrome.app.window.current();
        chrome.test.assertEq(cw, win);
        chrome.test.assertEq('testId', cw.id);
        win.contentWindow.close();
      }))
    },

    function loadEvent() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        win.contentWindow.onload = callbackPass(function() {
          chrome.test.assertEq(document.readyState, 'complete');
          win.contentWindow.close();
        });
      }));
    },

    function multiWindow() {
      chrome.test.assertTrue(null === chrome.app.window.current());
      chrome.app.window.create('test.html',
                               {id: 'testId1'},
                               callbackPass(function(win1) {
        chrome.app.window.create('test.html',
                                 {id: 'testId2'},
                                 callbackPass(function(win2) {
          var cw1 = win1.contentWindow.chrome.app.window.current();
          var cw2 = win2.contentWindow.chrome.app.window.current();
          chrome.test.assertEq('testId1', cw1.id);
          chrome.test.assertEq('testId2', cw2.id);
          chrome.test.assertTrue(cw1 === win1);
          chrome.test.assertTrue(cw2 === win2);
          chrome.test.assertFalse(cw1 === cw2);
          win1.contentWindow.close();
          win2.contentWindow.close();
        }));
      }));
    },

    function hiddenAndNormal() {
      chrome.app.window.create('test.html',
                               {hidden: true},
                               callbackPass(function(win1) {
        chrome.app.window.create('test.html',
                                 {hidden: false},
                                 callbackPass(function(win2) {
          win1.contentWindow.close();
          win2.contentWindow.close();
        }));
      }));
    },
    function sameWindowIdInitializesProperly() {
      // Regression test for http://crbug.com/943710
      // Both windows with the same id should be initialized
      let callback_fires = 0;
      chrome.app.window.create('test.html', { id: '1' },
        callbackPass(function (w) {
          chrome.test.assertTrue('contentWindow' in w);
          if (++callback_fires == 2) w.contentWindow.close();
        }));
      chrome.app.window.create('test.html', { id: '1' }, callbackPass(function (w) {
        chrome.test.assertTrue('contentWindow' in w);
        if (++callback_fires == 2) w.contentWindow.close();
      }));

    }
  ]);
}

function testDeprecatedBounds() {
  chrome.test.runTests([
    function contentSize() {
      var options = { bounds: { left: 0, top: 50, width: 250, height: 200 } };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.bounds.width, bounds.width);
        chrome.test.assertEq(options.bounds.height, bounds.height);
        chrome.test.assertEq(options.bounds.width, win.innerBounds.width);
        chrome.test.assertEq(options.bounds.height, win.innerBounds.height);
        win.close();
      }));
    },

    function windowPosition() {
      var options = { bounds: { left: 0, top: 50, width: 250, height: 200 } };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.bounds.left, bounds.left);
        chrome.test.assertEq(options.bounds.top, bounds.top);
        chrome.test.assertEq(options.bounds.left, win.outerBounds.left);
        chrome.test.assertEq(options.bounds.top, win.outerBounds.top);
        win.close();
      }));
    },

    function minSize() {
      var options = {
        bounds: { left: 0, top: 50, width: 250, height: 250 },
        minWidth: 400, minHeight: 450
      };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.minWidth, bounds.width);
        chrome.test.assertEq(options.minHeight, bounds.height);
        win.close();
      }));
    },

    function maxSize() {
      var options = {
        bounds: { left: 0, top: 50, width: 250, height: 250 },
        maxWidth: 200, maxHeight: 150
      };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.maxWidth, bounds.width);
        chrome.test.assertEq(options.maxHeight, bounds.height);
        win.close();
      }));
    },

    function minAndMaxSize() {
      var options = {
        bounds: { left: 0, top: 50, width: 250, height: 250 },
        minWidth: 400, minHeight: 450,
        maxWidth: 200, maxHeight: 150
      };
      chrome.app.window.create('test.html', options, callbackPass(
      function(win) {
        var bounds = win.getBounds();
        chrome.test.assertEq(options.minWidth, bounds.width);
        chrome.test.assertEq(options.minHeight, bounds.height);
        win.close();
      }));
    },

    function simpleSetBounds() {
      chrome.app.window.create('test.html', {
        bounds: { left: 0, top: 50, width: 250, height: 200 }
      }, callbackPass(function(win) {
        var newBounds = {width: 400, height: 450};
        win.setBounds(newBounds);
        chrome.test.waitForRoundTrip('msg', callbackPass(function() {
          var bounds = win.getBounds();
          chrome.test.assertEq(newBounds.width, bounds.width);
          chrome.test.assertEq(newBounds.height, bounds.height);
          win.close();
        }));
      }));
    },

    function heightOnlySetBounds() {
      chrome.app.window.create('test.html', {
        bounds: { left: 0, top: 50, width: 300, height: 256 }
      }, callbackPass(function(win) {
        win.setBounds({ height: 300 });
        chrome.test.waitForRoundTrip('msg', callbackPass(function() {
          var bounds = win.getBounds();
          chrome.test.assertEq(300, bounds.width);
          chrome.test.assertEq(300, bounds.height);
          win.close();
        }));
      }));
    },
  ]);
}

function testInitialBounds() {
  chrome.test.runTests([
    function testNoOptions() {
      chrome.app.window.create('test.html', {
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertTrue(win.innerBounds.width > 0);
        chrome.test.assertTrue(win.innerBounds.height > 0);
        chrome.test.assertTrue(win.outerBounds.width > 0);
        chrome.test.assertTrue(win.outerBounds.height > 0);
        assertConstraintsUnspecified(win);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testInnerBoundsOnly() {
      var innerBounds = {
        left: 150,
        top: 100,
        width: 400,
        height: 300
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        assertBoundsEq(innerBounds, win.innerBounds);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    // Regression for crbug.com/694248.
    function testInnerBoundsNegativeZero() {
      var innerBounds = {
        left: -0,
        top: 100,
        width: 400,
        height: 300,
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        assertBoundsEq(innerBounds, win.innerBounds);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testOuterBoundsOnly() {
      var outerBounds = {
        left: 150,
        top: 100,
        width: 400,
        height: 300
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        assertBoundsEq(outerBounds, win.outerBounds);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testFrameless() {
      var outerBounds = {
        left: 150,
        top: 100,
        width: 400,
        height: 300
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds,
        frame: 'none'
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        assertBoundsEq(outerBounds, win.outerBounds);
        assertBoundsEq(outerBounds, win.innerBounds);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testInnerSizeAndOuterPos() {
      var innerBounds = {
        width: 400,
        height: 300
      };
      var outerBounds = {
        left: 150,
        top: 100
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds,
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.left, win.outerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertEq(innerBounds.width, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.height, win.innerBounds.height);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testInnerAndOuterBoundsEdgeCase() {
      var innerBounds = {
        left: 150,
        height: 300
      };
      var outerBounds = {
        width: 400,
        top: 100
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds,
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.left, win.innerBounds.left);
        chrome.test.assertEq(innerBounds.height, win.innerBounds.height);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertEq(outerBounds.width, win.outerBounds.width);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testPositionOnly() {
      var outerBounds = {
        left: 150,
        top: 100
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.left, win.outerBounds.left);
        chrome.test.assertEq(outerBounds.top, win.outerBounds.top);
        chrome.test.assertTrue(win.innerBounds.width > 0);
        chrome.test.assertTrue(win.innerBounds.height > 0);
        chrome.test.assertTrue(win.outerBounds.width > 0);
        chrome.test.assertTrue(win.outerBounds.height > 0);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testSizeOnly() {
      var outerBounds = {
        width: 500,
        height: 400
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.width, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.height, win.outerBounds.height);
        assertBoundsConsistent(win);
        assertConstraintsUnspecified(win);
        win.close();
      }));
    },

    function testConflictingProperties() {
      testConflictingBoundsProperty("width");
      testConflictingBoundsProperty("height");
      testConflictingBoundsProperty("left");
      testConflictingBoundsProperty("top");
      testConflictingBoundsProperty("minWidth");
      testConflictingBoundsProperty("minHeight");
      testConflictingBoundsProperty("maxWidth");
      testConflictingBoundsProperty("maxHeight");
    }
  ]);
}

function testInitialConstraints() {
  chrome.test.runTests([
    function testMaxInnerConstraints() {
      var innerBounds = {
        width: 800,
        height: 600,
        maxWidth: 500,
        maxHeight: 400
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.maxWidth, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.maxHeight, win.innerBounds.height);
        chrome.test.assertEq(innerBounds.maxWidth, win.innerBounds.maxWidth);
        chrome.test.assertEq(innerBounds.maxHeight, win.innerBounds.maxHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMinInnerConstraints() {
      var innerBounds = {
        width: 100,
        height: 100,
        minWidth: 300,
        minHeight: 200
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(innerBounds.minWidth, win.innerBounds.width);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.height);
        chrome.test.assertEq(innerBounds.minWidth, win.innerBounds.minWidth);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.minHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMaxOuterConstraints() {
      var outerBounds = {
        width: 800,
        height: 600,
        maxWidth: 500,
        maxHeight: 400
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.maxWidth, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.maxHeight, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.maxWidth, win.outerBounds.maxWidth);
        chrome.test.assertEq(outerBounds.maxHeight, win.outerBounds.maxHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMinOuterConstraints() {
      var outerBounds = {
        width: 100,
        height: 100,
        minWidth: 300,
        minHeight: 200
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.minHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testMixedConstraints() {
      var innerBounds = {
        width: 100,
        minHeight: 300
      };
      var outerBounds = {
        height: 100,
        minWidth: 400,
      };
      chrome.app.window.create('test.html', {
        innerBounds: innerBounds,
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.width);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.height);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(innerBounds.minHeight, win.innerBounds.minHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testBadConstraints() {
      var outerBounds = {
        width: 500,
        height: 400,
        minWidth: 800,
        minHeight: 700,
        maxWidth: 300,
        maxHeight: 200
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.width);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.height);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.minWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.minHeight);
        chrome.test.assertEq(outerBounds.minWidth, win.outerBounds.maxWidth);
        chrome.test.assertEq(outerBounds.minHeight, win.outerBounds.maxHeight);
        assertBoundsConsistent(win);
        win.close();
      }));
    },

    function testFrameless() {
      var outerBounds = {
        minWidth: 50,
        minHeight: 50,
        maxWidth: 800,
        maxHeight: 800
      };
      chrome.app.window.create('test.html', {
        outerBounds: outerBounds,
        frame: 'none'
      }, callbackPass(function(win) {
        chrome.test.assertTrue(win != null);
        assertConstraintsEq(outerBounds, win.outerBounds);
        assertConstraintsEq(outerBounds, win.innerBounds);
        win.close();
      }));
    }
  ]);
}

function testSetBounds() {
  chrome.test.runTests([
    function testLeft() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { left: 189 };
      var expected = { left: 189, top: 100, width: 300, height: 200 };
      runSetBoundsTest('innerBounds', init, change, expected);
      runSetBoundsTest('outerBounds', init, change, expected);
    },

    function testLeftNull() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { left: null };
      runSetBoundsTest('innerBounds', init, change, init);
      runSetBoundsTest('outerBounds', init, change, init);
    },

    function testTop() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { top: 167 };
      var expected = { left: 150, top: 167, width: 300, height: 200 };
      runSetBoundsTest('innerBounds', init, change, expected);
      runSetBoundsTest('outerBounds', init, change, expected);
    },

    function testTopNull() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { top: null };
      runSetBoundsTest('innerBounds', init, change, init);
      runSetBoundsTest('outerBounds', init, change, init);
    },

    function testWidth() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { width: 245 };
      var expected = { left: 150, top: 100, width: 245, height: 200 };
      runSetBoundsTest('innerBounds', init, change, expected);
      runSetBoundsTest('outerBounds', init, change, expected);
    },

    function testWidthNull() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { width: null };
      runSetBoundsTest('innerBounds', init, change, init);
      runSetBoundsTest('outerBounds', init, change, init);
    },

    function testHeight() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { height: 196 };
      var expected = { left: 150, top: 100, width: 300, height: 196 };
      runSetBoundsTest('innerBounds', init, change, expected);
      runSetBoundsTest('outerBounds', init, change, expected);
    },

    function testHeightNull() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { height: null };
      runSetBoundsTest('innerBounds', init, change, init);
      runSetBoundsTest('outerBounds', init, change, init);
    },

    function testPosition() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { left: 162, top: 112 };
      var expected = { left: 162, top: 112, width: 300, height: 200 };
      runSetBoundsTest('innerBounds', init, change, expected);
      runSetBoundsTest('outerBounds', init, change, expected);
    },

    function testPositionNull() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { left: null, top: null };
      runSetBoundsTest('innerBounds', init, change, init);
      runSetBoundsTest('outerBounds', init, change, init);
    },

    function testSize() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { width: 306, height: 216 };
      var expected = { left: 150, top: 100, width: 306, height: 216 };
      runSetBoundsTest('innerBounds', init, change, expected);
      runSetBoundsTest('outerBounds', init, change, expected);
    },

    function testSizeNull() {
      var init = { left: 150, top: 100, width: 300, height: 200 };
      var change = { width: null, height: null };
      runSetBoundsTest('innerBounds', init, change, init);
      runSetBoundsTest('outerBounds', init, change, init);
    },

    function testMinSize() {
      var init = { left: 150, top: 100, width: 300, height: 200,
                   minWidth: 235, minHeight: 170 };
      var change = { width: 50, height: 60 };
      var expected = { left: 150, top: 100, width: 235, height: 170 };
      runSetBoundsTest('innerBounds', init, change, expected, true);
      runSetBoundsTest('outerBounds', init, change, expected, true);
    },

    function testMaxSize() {
      var init = { left: 150, top: 100, width: 300, height: 200,
                   maxWidth: 330, maxHeight: 230 };
      var change = { width: 400, height: 300 };
      var expected = { left: 150, top: 100, width: 330, height: 230 };
      runSetBoundsTest('innerBounds', init, change, expected, true);
      runSetBoundsTest('outerBounds', init, change, expected, true);
    },

    function testMinAndMaxSize() {
      var init = { left: 150, top: 100, width: 300, height: 200,
                   minWidth: 120, minHeight: 170,
                   maxWidth: 330, maxHeight: 230 };
      var change = { width: 225, height: 195 };
      var expected = { left: 150, top: 100, width: 225, height: 195 };
      runSetBoundsTest('innerBounds', init, change, expected, true);
      runSetBoundsTest('outerBounds', init, change, expected, true);
    },
  ]);
}

function testSetSizeConstraints() {
  chrome.test.runTests([
    function testMinWidth() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { minWidth: 111 };
      var expected = { minWidth: 111, minHeight: 200,
                       maxWidth: 350, maxHeight: 250 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testClearMinWidth() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { minWidth: null };
      var expected = { minWidth: null, minHeight: 200,
                       maxWidth: 350, maxHeight: 250 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testMaxWidth() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { maxWidth: 347 };
      var expected = { minWidth: 300, minHeight: 200,
                       maxWidth: 347, maxHeight: 250 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testClearMaxWidth() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { maxWidth: null };
      var expected = { minWidth: 300, minHeight: 200,
                       maxWidth: null, maxHeight: 250 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testMinHeight() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { minHeight: 198 };
      var expected = { minWidth: 300, minHeight: 198,
                       maxWidth: 350, maxHeight: 250 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testClearMinHeight() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { minHeight: null };
      var expected = { minWidth: 300, minHeight: null,
                       maxWidth: 350, maxHeight: 250 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testMaxHeight() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { maxHeight: 278 };
      var expected = { minWidth: 300, minHeight: 200,
                       maxWidth: 350, maxHeight: 278 };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testClearMaxHeight() {
      var init = { minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { maxHeight: null };
      var expected = { minWidth: 300, minHeight: 200,
                       maxWidth: 350, maxHeight: null };
      runSetConstraintsTest('innerBounds', init, change, expected);
      runSetConstraintsTest('outerBounds', init, change, expected);
    },

    function testSetMinSize() {
      // This test expects the bounds to be changed.
      var init = { width: 225, height: 125,
                   minWidth: null, minHeight: null,
                   maxWidth: null, maxHeight: null };
      var change = { minWidth: 235, minHeight: 135 };
      var expected = { width: 235, height: 135,
                       minWidth: 235, minHeight: 135,
                       maxWidth: null, maxHeight: null };
      runSetConstraintsTest('innerBounds', init, change, expected, expected);
      runSetConstraintsTest('outerBounds', init, change, expected, expected);
    },

    function testSetMaxSize() {
      // This test expects the bounds to be changed.
      var init = { width: 225, height: 125,
                   minWidth: null, minHeight: null,
                   maxWidth: null, maxHeight: null };
      var change = { maxWidth: 198, maxHeight: 107 };
      var expected = { width: 198, height: 107,
                       minWidth: null, minHeight: null,
                       maxWidth: 198, maxHeight: 107 };
      runSetConstraintsTest('innerBounds', init, change, expected, expected);
      runSetConstraintsTest('outerBounds', init, change, expected, expected);
    },

    function testChangeMinAndMaxSize() {
      var init = { width: 325, height: 225,
                   minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { minWidth: 287, minHeight: 198,
                     maxWidth: 334, maxHeight: 278 };
      runSetConstraintsTest('innerBounds', init, change, change, init);
      runSetConstraintsTest('outerBounds', init, change, change, init);
    },

    function testClearMinAndMaxSize() {
      var init = { width: 325, height: 225,
                   minWidth: 300, minHeight: 200,
                   maxWidth: 350, maxHeight: 250 };
      var change = { minWidth: null, minHeight: null,
                     maxWidth: null, maxHeight: null };
      runSetConstraintsTest('innerBounds', init, change, change, init);
      runSetConstraintsTest('outerBounds', init, change, change, init);
    },

    function testClearConstraints() {
      // This checks that bounds are not clipped once constraints are removed.
      var createOptions = {
        innerBounds: {
          width: 325, height: 225,
          minWidth: 300, minHeight: 200,
          maxWidth: 350, maxHeight: 250
        }
      };
      chrome.app.window.create('test.html', createOptions, callbackPass(
      function(win) {
        win.innerBounds.setMinimumSize(null, null);
        win.innerBounds.setMaximumSize(null, null);

        // Set the size smaller than the initial min.
        win.innerBounds.setSize(234, 198);

        // Dummy call to wait for bounds to be changed in the browser.
        chrome.test.waitForRoundTrip('msg', callbackPass(function(msg) {
          chrome.test.assertEq(234, win.innerBounds.width);
          chrome.test.assertEq(198, win.innerBounds.height);

          // Set the size larger than the initial max.
          win.innerBounds.setSize(361, 278);

          chrome.test.waitForRoundTrip('msg', callbackPass(function(msg) {
            chrome.test.assertEq(361, win.innerBounds.width);
            chrome.test.assertEq(278, win.innerBounds.height);
            win.close();
          }));
        }));
      }));
    },

    function testMinWidthLargerThanMaxWidth() {
      var init = { width: 102, height: 103,
                   minWidth: 100, minHeight: 101,
                   maxWidth: 104, maxHeight: 105 };
      var change = { minWidth: 200 };
      var expected = { minWidth: 200, minHeight: 101,
                       maxWidth: 200, maxHeight: 105 };
      runSetConstraintsTest('innerBounds', init, change, expected);
    },

    function testMinHeightLargerThanMaxHeight() {
      var init = { width: 102, height: 103,
                   minWidth: 100, minHeight: 101,
                   maxWidth: 104, maxHeight: 105 };
      var change = { minHeight: 200 };
      var expected = { minWidth: 100, minHeight: 200,
                       maxWidth: 104, maxHeight: 200 };
      runSetConstraintsTest('innerBounds', init, change, expected);
    },

    function testMaxWidthSmallerThanMinWidth() {
      var init = { width: 102, height: 103,
                   minWidth: 100, minHeight: 101,
                   maxWidth: 104, maxHeight: 105 };
      var change = { maxWidth: 50 };
      var expected = { minWidth: 100, minHeight: 101,
                       maxWidth: 100, maxHeight: 105 };
      runSetConstraintsTest('innerBounds', init, change, expected);
    },

    function testMaxHeightSmallerThanMinHeight() {
      var init = { width: 102, height: 103,
                   minWidth: 100, minHeight: 101,
                   maxWidth: 104, maxHeight: 105 };
      var change = { maxHeight: 50 };
      var expected = { minWidth: 100, minHeight: 101,
                       maxWidth: 104, maxHeight: 101 };
      runSetConstraintsTest('innerBounds', init, change, expected);
    },
  ]);
}

function testSingleton() {
  chrome.test.runTests([
    function noParameterWithId() {
      chrome.app.window.create(
        'test.html', { id: 'singleton-id' },
        callbackPass(function(win) {
          var w = win.contentWindow;

          chrome.app.window.create(
            'test.html', { id: 'singleton-id' },
            callbackPass(function(win) {
              var w2 = win.contentWindow;
              chrome.test.assertTrue(w === w2);

              w.close();
              w2.close();
            })
          );
        })
      );
    },
  ]);
}

function testCloseEvent() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        win.onClosed.addListener(callbackPass(function() {
          // Mission accomplished.
        }));
        win.contentWindow.close();
      }));
    }
  ]);
}

function testMaximize() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html',
                               { innerBounds: {width: 200, height: 200} },
        callbackPass(function(win) {
          // TODO(mlamouri): we should be able to use onMaximized here but to
          // make that happen we need to make sure the event is not fired when
          // .maximize() is called but when the maximizing is finished.
          // See crbug.com/316091
          function isWindowMaximized() {
            return win.contentWindow.outerHeight == screen.availHeight &&
                   win.contentWindow.outerWidth == screen.availWidth;
          }

          eventLoopCheck(isWindowMaximized, function() {
            win.close();
          });

          win.maximize();
        })
      );
    },

    function nonResizableWindow() {
      chrome.app.window.create('test.html',
                               { innerBounds: {width: 200, height: 200},
                                 resizable: false },
        callbackPass(function(win) {
          // TODO(mlamouri): we should be able to use onMaximized here but to
          // make that happen we need to make sure the event is not fired when
          // .maximize() is called but when the maximizing is finished.
          // See crbug.com/316091
          function isWindowMaximized() {
            return win.contentWindow.outerHeight == screen.availHeight &&
                   win.contentWindow.outerWidth == screen.availWidth;
          }

          eventLoopCheck(isWindowMaximized, function() {
            win.close();
          });

          win.maximize();
        })
      );
    },
  ]);
}

function testMinimize() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html',
                               { innerBounds: {width: 200, height: 200} },
        callbackPass(function(win) {
          function isWindowMinimized() {
            return win.isMinimized();
          }

          win.minimize();
          eventLoopCheck(isWindowMinimized, function() {
            win.close();
          });
        })
      );
    },

    function checkSizeAfterRestore() {
      var bounds = { width: 200, height: 200,
                     minWidth: 200, minHeight: 200,
                     maxWidth: 200, maxHeight: 200 };
      chrome.app.window.create('test.html', { innerBounds: bounds },
        callbackPass(function(win) {
          function isWindowMinimized() {
            return win.isMinimized();
          }

          function sizeIsSame() {
            return bounds.width == win.innerBounds.width &&
                   bounds.height == win.innerBounds.height;
          }

          win.minimize();
          eventLoopCheck(isWindowMinimized, function() {
            win.restore();
            eventLoopCheck(sizeIsSame, function() {
              win.close();
            });
          });
        })
      );
    },
  ]);
}

function testRestore() {
  chrome.test.runTests([
    function basic() {
      chrome.app.window.create('test.html',
                               { innerBounds: {width: 200, height: 200} },
        callbackPass(function(win) {
          var oldWidth = win.contentWindow.innerWidth;
          var oldHeight = win.contentWindow.innerHeight;

          // TODO(mlamouri): we should be able to use onMaximized here but to
          // make that happen we need to make sure the event is not fired when
          // .maximize() is called but when the maximizing is finished.
          // See crbug.com/316091
          function isWindowMaximized() {
            return win.contentWindow.outerHeight == screen.availHeight &&
                   win.contentWindow.outerWidth == screen.availWidth;
          }
          function isWindowRestored() {
            return win.contentWindow.innerHeight == oldHeight &&
                   win.contentWindow.innerWidth == oldWidth;
          }

          eventLoopCheck(isWindowMaximized, function() {
            eventLoopCheck(isWindowRestored, function() {
              win.close();
            });

            win.restore();
          });

          win.maximize();
        })
      );
    }
  ]);
}

function testRestoreAfterClose() {
  chrome.test.runTests([
    function restoredBoundsLowerThanNewMinSize() {
      chrome.app.window.create('test.html', {
        innerBounds: {
          width: 100, height: 150,
          minWidth: 200, minHeight: 250,
          maxWidth: 200, maxHeight: 250
        },
        id: 'test-id'
      }, callbackPass(function(win) {
        var w = win.contentWindow;
        assertFuzzyEq(200, w.innerWidth, defaultFuzzFactor);
        assertFuzzyEq(250, w.innerHeight, defaultFuzzFactor);

        win.onClosed.addListener(callbackPass(function() {
          chrome.app.window.create('test.html', {
            innerBounds: {
              width: 500, height: 550,
              minWidth: 400, minHeight: 450,
              maxWidth: 600, maxHeight: 650
            },
            id: 'test-id'
          }, callbackPass(function(win) {
            var w = win.contentWindow;
            assertFuzzyEq(400, w.innerWidth, defaultFuzzFactor);
            assertFuzzyEq(450, w.innerHeight, defaultFuzzFactor);
            w.close();
          }));
        }));

        w.close();
      }));
    }
  ]);
}

function testRestoreAfterGeometryCacheChange() {
  chrome.test.runTests([
    function restorePositionAndSize() {
      chrome.app.window.create('test.html', {
        outerBounds: { left: 200, top: 200 },
        innerBounds: { width: 200, height: 200 },
        id: 'test-ra',
      }, callbackPass(function(win) { waitForLoad(win, function(win) {
        var w = win.contentWindow;
        chrome.test.assertEq(200, w.screenX);
        chrome.test.assertEq(200, w.screenY);
        chrome.test.assertEq(200, w.innerHeight);
        chrome.test.assertEq(200, w.innerWidth);

        w.resizeTo(300, 300);
        w.moveTo(100, 100);

        chrome.app.window.create('test.html', {
          outerBounds: { left: 200, top: 200, width: 200, height: 200 },
          id: 'test-rb', frame: 'none'
        }, callbackPass(function(win2) { waitForLoad(win2, function(win2) {
          var w2 = win2.contentWindow;
          chrome.test.assertEq(200, w2.screenX);
          chrome.test.assertEq(200, w2.screenY);
          chrome.test.assertEq(200, w2.innerWidth);
          chrome.test.assertEq(200, w2.innerHeight);

          w2.resizeTo(100, 100);
          w2.moveTo(300, 300);

          chrome.test.sendMessage('ListenGeometryChange', function(reply) {
            win.onClosed.addListener(callbackPass(function() {
              chrome.app.window.create('test.html', {
                id: 'test-ra'
              }, callbackPass(function(win) { waitForLoad(win, function(win) {
                var w = win.contentWindow;
                chrome.test.assertEq(100, w.screenX);
                chrome.test.assertEq(100, w.screenY);
                chrome.test.assertEq(300, w.outerWidth);
                chrome.test.assertEq(300, w.outerHeight);
              })}));
            }));

            win2.onClosed.addListener(callbackPass(function() {
              chrome.app.window.create('test.html', {
                id: 'test-rb', frame: 'none'
              },callbackPass(function(win2) { waitForLoad(win2, function(win2) {
                var w = win2.contentWindow;
                chrome.test.assertEq(300, w.screenX);
                chrome.test.assertEq(300, w.screenY);
                chrome.test.assertEq(100, w.outerWidth);
                chrome.test.assertEq(100, w.outerHeight);
              })}));
            }));

            win.close();
            win2.close();
          });
        })}));
      })}));
    },
  ]);
}

function testFrameColors() {
  chrome.test.runTests([
    function testWithNoColor() {
      chrome.app.window.create('test.html', callbackPass(function(win) {
        chrome.test.assertEq(false, win.hasFrameColor);
        win.close();
      }));
    },

    function testWithFrameNone() {
      chrome.app.window.create('test.html', {
        frame: 'none'
      },
      callbackPass(function(win) {
        chrome.test.assertEq(false, win.hasFrameColor);
        win.close();
      }));
    },

    function testWithBlack() {
      chrome.app.window.create('test.html', {
        frame: {
          type: 'chrome',
          color: '#000000'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(0x000000, win.activeFrameColor);
        chrome.test.assertEq(0x000000, win.inactiveFrameColor);
        win.close();
      }));
    },

    function testWithWhite() {
      chrome.app.window.create('test.html', {
        frame: {
          color: '#FFFFFF'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(0xFFFFFF, win.activeFrameColor);
        chrome.test.assertEq(0xFFFFFF, win.inactiveFrameColor);
        win.close();
      }));
    },

    function testWithActiveInactive() {
      chrome.app.window.create('test.html', {
        frame: {
          type: 'chrome',
          color: '#000000',
          inactiveColor: '#FFFFFF'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(0x000000, win.activeFrameColor);
        chrome.test.assertEq(0xFFFFFF, win.inactiveFrameColor);
        win.close();
      }));
    },

    function testWithWhiteShorthand() {
      chrome.app.window.create('test.html', {
        frame: {
          color: '#FFF'
        }
      },
      callbackPass(function(win) {
        chrome.test.assertEq(true, win.hasFrameColor);
        chrome.test.assertEq(0xFFFFFF, win.activeFrameColor);
        chrome.test.assertEq(0xFFFFFF, win.inactiveFrameColor);
        win.close();
      }));
    },

    function testWithFrameNoneAndColor() {
      chrome.app.window.create('test.html', {
        frame: {
          type: 'none',
          color: '#FFF'
        }
      },
      callbackFail('Windows with no frame cannot have a color.'));
    },

    function testWithInactiveColorAndNoColor() {
      chrome.app.window.create('test.html', {
        frame: {
          inactiveColor: '#FFF'
        }
      },
      callbackFail('frame.inactiveColor must be used with frame.color.'));
    },

     function testWithInvalidColor() {
      chrome.app.window.create('test.html', {
        frame: {
          color: 'DontWorryBeHappy'
        }
      },
      callbackFail('The color specification could not be parsed.'));
    }
  ]);
}

function testVisibleOnAllWorkspaces() {
  chrome.test.runTests([
    function setAndUnsetVisibleOnAllWorkspaces() {
      chrome.app.window.create('test.html', {
        visibleOnAllWorkspaces: true
      }, callbackPass(function(win) {
        win.setVisibleOnAllWorkspaces(false);
        win.setVisibleOnAllWorkspaces(true);
        chrome.test.sendMessage(
            'WaitForRoundTrip', callbackPass(function(reply) {}));
      }));
    },
  ]);
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage('Launched', function(reply) {
    window[reply]();
  });
});
