// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var apiFeatures = chrome.test.getApiFeatures();

// Returns a list of all chrome.foo.bar API paths available to an app.
function getApiPaths() {
  var apiPaths = [];
  var apiDefinitions = chrome.test.getApiDefinitions();
  apiDefinitions.forEach(function(module) {
    var namespace = module.namespace;
    var apiFeature = apiFeatures[namespace];
    if (Array.isArray(apiFeature))
      apiFeature = apiFeatures[namespace][0];

    // Skip internal APIs.
    if (apiFeature.internal)
      return;

    // Get the API functions and events.
    [module.functions, module.events].forEach(function(section) {
      if (typeof(section) == "undefined")
        return;
      // Pieces of the module don't inherit from Array/Object.
      Array.prototype.forEach.call(section, function(entry) {
        // Skip idle.getAutoLockDelay() and power.reportActivity() since they
        // are restricted to certain platforms.
        // TODO(https://crbug.com/921466)
        if (entry.name != 'getAutoLockDelay' &&
            entry.name != 'reportActivity') {
          apiPaths.push(namespace + "." + entry.name);
        }
      });
    });

    // Get the API properties.
    if (module.properties) {
      Object.getOwnPropertyNames(module.properties).forEach(function(propName) {
        const fullPath = namespace + '.' + propName;
        // Skip storage.session, since it's restricted to MV3.
        if (fullPath != 'storage.session') {
          apiPaths.push(fullPath);
        }
      });
    }
  });
  return apiPaths;
}

// Tests whether all parts of an API path can be accessed. The path is a
// namespace or function/property/event etc. within a namespace, and is
// dot-separated.
function testPath(path) {
  var parts = path.split('.');

  var module = chrome;
  for (var i = 0; i < parts.length; i++) {
    // Touch this component of the path. This will die if an API does not have
    // a schema registered.
    module = module[parts[i]];

    // The component should be defined unless it is lastError, which depends on
    // there being an error.
    if (typeof(module) == "undefined" && path != "runtime.lastError")
      return false;
  }
  return true;
}

function doTest() {
  // Run over all API path strings and ensure each path is defined.
  var failures = [];
  getApiPaths().forEach(function(path) {
    if (!testPath(path)) {
      failures.push(path);
    }
  });

  // Lack of failure implies success.
  if (failures.length == 0) {
    chrome.test.notifyPass();
  } else {
    console.log("failures on:\n" + failures.join("\n") +
        "\n\n\n>>> See comment in stubs_apitest.cc for a " +
        "hint about fixing this failure.\n\n");
    chrome.test.notifyFail("failed");
  }
}

chrome.app.runtime.onLaunched.addListener(function() {
  doTest();
});
