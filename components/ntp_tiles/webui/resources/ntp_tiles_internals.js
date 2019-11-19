// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.ntp_tiles_internals', function() {
  'use strict';

  var initialize = function() {
    $('submit-update').addEventListener('click', function(event) {
      event.preventDefault();
      chrome.send('update', [{
        "popular": {
          "overrideURL": $('override-url').value,
          "overrideDirectory": $('override-directory').value,
          "overrideCountry": $('override-country').value,
          "overrideVersion": $('override-version').value,
        },
      }]);
    });

    $('suggestions-fetch').addEventListener('click', function(event) {
      event.preventDefault();
      chrome.send('fetchSuggestions');
    });

    $('popular-view-json').addEventListener('click', function(event) {
      event.preventDefault();
      if ($('popular-json-value').textContent === "") {
        chrome.send('viewPopularSitesJson');
      } else {
        $('popular-json-value').textContent = "";
      }
    });

    chrome.send('registerForEvents');
  };

  var receiveSourceInfo = function(state) {
    jstProcess(new JsEvalContext(state), $('sources'));
  };

  var receiveSites = function(sites) {
    jstProcess(new JsEvalContext(sites), $('sites'));
  };

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    receiveSourceInfo: receiveSourceInfo,
    receiveSites: receiveSites,
  };
});

document.addEventListener('DOMContentLoaded',
                          chrome.ntp_tiles_internals.initialize);
