// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script file should be requested with the following query params:
// subresourceHost: The base url of the embedded test server to request
// subresources from.
// numCorsResources: The number of resources to request (between 0 and 3).
var queryString = window.location.search.substring(1);
var vars = queryString.split("&");
var parsedParams = {};
vars.forEach(function(v) {
  var key_value = v.split('=');
  parsedParams[key_value[0]] = key_value[1];
});
var subresourceHost = parsedParams['subresourceHost'];
var numCorsResources = parseInt(parsedParams['numCorsResources'], 10);
var sendImmediately = parseInt(parsedParams['sendImmediately'], 10);
var subresourceList = [
  'predictor/empty.js',
  'predictor/empty1.js',
  'predictor/empty2.js',
];
var CorsSubresources = subresourceList.slice(0, numCorsResources);

var numOK = 0;
function logResponse(r) {
  // Opaque responses are not necessarily network errors. We get them when we
  // make cross origin requests with no-cors.
  if (r.status == 200 || r.type == "opaque") {
    numOK++;
    if (numOK == numCorsResources) {
      window.domAutomationController.send(true);
    }
  } else {
    window.domAutomationController.send(false);
  }
}

function startFetchesAndWaitForReply() {
  CorsSubresources.forEach(function(r) {
    fetch(subresourceHost + r, {mode: 'cors', method: 'get'}).then(logResponse);
  });
}

if (sendImmediately) {
  startFetchesAndWaitForReply();
}
