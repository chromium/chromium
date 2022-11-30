// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fetch(chrome.runtime.getURL('data_for_content_script'))
  .then(function(res) { return res.text(); })
  .then(function(txt) {
      if (txt != 'original data\n')
        throw 'Fetch() result error: ' + txt;
      return new Promise(function(resolve) {
          var xhr = new XMLHttpRequest();
          xhr.addEventListener('load', function() {
              resolve(xhr.response);
            });
          xhr.open('GET', chrome.runtime.getURL('data_for_content_script'));
          xhr.send();
        });
    })
  .then(function(txt) {
      if (txt != 'original data\n')
        throw 'XMLHttpRequest result error: ' + txt;
      chrome.runtime.connect().postMessage('Success');
    })
  .catch(function(e) {
      chrome.runtime.connect().postMessage('Failure: ' + e);
    });
