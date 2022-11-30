// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var startTest = function(testDir) {
  LOG('startTest: ' + testDir);
  // load bootstrap script.
  var script = document.createElement('script');
  var scriptPath = testDir + '/bootstrap.js';
  script.type = 'text/javascript';
  script.src = scriptPath;
  document.getElementsByTagName('head')[0].appendChild(script);

  script.addEventListener('error', function(e) {
    // This is mostly for debugging, e.g. if you specify an incorrect path
    // for guest bootstrap.js script.
    window.console.log('Error in loading guest script from path: ' +
                       scriptPath +
                       ', Possibly misspelled path?');
    utils.test.fail();
  });
};

var onloadFunction = function() {
  window.console.log('app.onload');
  chrome.test.getConfig(function(config) {
    LOG('embeder.common got config: ' + config);
    LOG('customArg: ' + config.customArg);
    loadFired = true;
    window['startTest'](config.customArg);
  });
};

onload = onloadFunction;

