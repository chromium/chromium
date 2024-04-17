// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper function to log message to both the local console and to the
// background page, so that the latter can output the message via the
// chrome.test.log() function.
function logToConsoleAndStdout(msg) {
  console.log(msg);
  chrome.extension.sendRequest("log: " + msg);
}

// We ask the background page to get the extension API to test against. When it
// responds we start the test.
console.log("asking for api ...");
chrome.extension.sendRequest("getApi", function(apis) {
  var apiFeatures = chrome.test.getApiFeatures();
  // TODO(crbug.com/41478937): This really should support more than two levels
  // of inheritance.
  function isAvailableToContentScripts(namespace, path) {
    const results = {
      NULL: 'null',
      NOT_FOUND: 'not_found',
      FOUND: 'found'
    };
    function searchContexts(contexts) {
      // This is tricky because the context can be either:
      //   1. Undefined, so return results.NULL.
      //   2. The string value 'all', so return results.FOUND.
      //   3. An array of string values, which cannot be empty.
      //      Return results.FOUND if 'content_script' is in the array.
      if (!contexts) {
        return results.NULL;
      }
      if (contexts == 'all' || contexts.includes('content_script')) {
        return results.FOUND;
      }
      return results.NOT_FOUND;
    }

    function searchFeature(feature) {
      if (!feature.length) {
        // Simple feature, not an array. Can return results.NULL,
        // because feature.context may be undefined.
        return searchContexts(feature.contexts);
      }
      // Complex feature. We need to return results.NULL if we didn't
      // find any contexts.
      var foundContext = false;
      for (var i = 0; i < feature.length; ++i) {
        var currentResult = searchContexts(feature[i].contexts);
        if (currentResult === results.FOUND) {
          return results.FOUND;
        }
        foundContext = currentResult !== results.NULL;
      }
      return foundContext ? results.NOT_FOUND : results.NULL;
    }

    var pathFeature = apiFeatures[path];
    if (!!pathFeature) {
      const result = searchFeature(pathFeature);
      // If we found something, use that result.
      if (result !== results.NULL) {
          return result === results.FOUND;
      }
    }

    var namespaceFeature = apiFeatures[namespace];
    // Check the namespace, if it's defined.
    if (!!namespaceFeature) {
      return searchFeature(namespaceFeature) === results.FOUND;
    }
    return false;
  } /* isAvailableToContentScripts */

  console.log("got api response");
  var privilegedPaths = [];
  var unprivilegedPaths = [];
  apis.forEach(function(module) {
    var namespace = module.namespace;

    ["functions", "events"].forEach(function(section) {
      if (typeof(module[section]) == "undefined")
        return;
      module[section].forEach(function(entry) {
        // Ignore entries that are not applicable to the manifest that we're
        // running under.
        if (entry.maximumManifestVersion && entry.maximumManifestVersion < 2) {
          return;
        }

        var path = namespace + "." + entry.name;
        if (module.unprivileged || entry.unprivileged ||
            isAvailableToContentScripts(namespace, path)) {
          unprivilegedPaths.push(path);
        } else {
          privilegedPaths.push(path);
        }
      });
    });

    if (module.properties) {
      for (var propName in module.properties) {
        var path = namespace + "." + propName;
        if (module.unprivileged || module.properties[propName].unprivileged ||
            isAvailableToContentScripts(namespace, path)) {
          unprivilegedPaths.push(path);
        } else {
          privilegedPaths.push(path);
        }
      }
    }
  });
  doTest(privilegedPaths, unprivilegedPaths);
});


// Tests whether missing properties of the chrome object correctly throw an
// error on access. The path is a namespace or function/property/event etc.
// within a namespace, and is dot-separated.
function testPath(path, expectError) {
  var parts = path.split('.');

  var module = chrome;
  for (var i = 0; i < parts.length; i++) {
    if (i < parts.length - 1) {
      // Not the last component. Allowed to be undefined because some paths are
      // only defined on some platforms.
      module = module[parts[i]];
      if (typeof(module) == "undefined")
        return true;
    } else {
      // This is the last component - we expect it to either be undefined or
      // to throw an error on access.
      if (typeof(module[parts[i]]) == "undefined" &&
          // lastError being defined depends on there being an error obviously.
          path != "extension.lastError" &&
          path != "runtime.lastError") {
        if (expectError) {
          return true;
        } else {
          logToConsoleAndStdout(" fail (should not be undefined): " + path);
          return false;
        }
      } else if (!expectError) {
        return true;
      }
    }
  }
  logToConsoleAndStdout(" fail (no error when we were expecting one): " + path);
  return false;
}

function displayResult(status) {
  var div = document.createElement("div");
  div.innerHTML = "<h1>" + status + "</h2>";
  document.body.appendChild(div);
}

function reportSuccess() {
  displayResult("pass");
  chrome.extension.sendRequest("pass");
}

function reportFailure() {
  displayResult("fail");
  // Let the "fail" show for a little while so you can see it when running
  // browser_tests in the debugger.
  setTimeout(function() {
    chrome.extension.sendRequest("fail");
  }, 1000);
}

// Runs over each string path in privilegedPaths and unprivilegedPaths, testing
// to ensure a proper error is thrown on access or the path is defined.
function doTest(privilegedPaths, unprivilegedPaths) {
  console.log("starting");

  if (!privilegedPaths || privilegedPaths.length < 1 || !unprivilegedPaths ||
      unprivilegedPaths.length < 1) {
    reportFailure();
    return;
  }

  var failures = [];
  var success = true;

  // Returns a function that will test a path and record any failures.
  function makeTestFunction(expectError) {
    return function(path) {
      // runtime.connect and runtime.sendMessage are available in all contexts,
      // unlike the runtime API in general.
      var expectErrorForPath = expectError &&
                               path != 'runtime.connect' &&
                               path != 'runtime.sendMessage';
      if (!testPath(path, expectErrorForPath)) {
        success = false;
        failures.push(path);
      }
    };
  }
  privilegedPaths.forEach(makeTestFunction(true));
  unprivilegedPaths.forEach(makeTestFunction(false));

  console.log(success ? "pass" : "fail");
  if (success) {
    reportSuccess();
  } else {
    logToConsoleAndStdout("failures on:\n" + failures.join("\n") +
        "\n\n\n>>> See comment in stubs_apitest.cc for a " +
        "hint about fixing this failure.\n\n");
    reportFailure();
  }
}
