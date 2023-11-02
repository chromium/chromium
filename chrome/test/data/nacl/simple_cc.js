// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set up listeners and test expectations for tests which use simple.cc
// This depends on load_util.js for reporting.
var simple_test = {
  addTestListeners: function(embed) {
    embed.addEventListener("load", function(evt) {
      embed.postMessage("ping");
    }, true);

    embed.addEventListener("message", function(message) {
      var expected_message = "pong";
      if (message.data === expected_message) {
        load_util.report({type: "Shutdown",
                          message: "1 test passed.", passed: true});
      } else {
        load_util.report(
          {type: "Log",
           message: "Error: expected reply (" + expected_message +
                    "), actual reply (" + message + ")"});
        load_util.report({type: "Shutdown",
                          message: "1 test failed.", passed: false});
      }
    }, true);

    embed.addEventListener("error", function(evt) {
      load_util.report({type: "Log",
                        message: "Load error: " + embed.lastError});
      load_util.report({type: "Shutdown",
                        message: "1 test failed.", passed: false});
    }, true);

    embed.addEventListener("crash", function(evt) {
      load_util.report({type: "Log",
                        message: "Crashed with status: " + embed.exitStatus});
      load_util.report({type: "Shutdown",
                        message: "1 test failed.", passed: false});
    }, true);
  }
};
