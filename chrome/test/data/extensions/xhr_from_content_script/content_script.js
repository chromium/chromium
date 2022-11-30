// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add a button to the page that can fetch a cross-origin file.
var b = document.createElement('button');
b.id = 'xhrButton';
b.innerText = 'Send XHR';
document.body.appendChild(b);

// Do a cross-origin fetch to an origin not listed in this extension's manifest.
// The HTTP request will have an Origin: HTTP header with a chrome-extension://
// security origin. Notifies the test if it succeeds or fails.
b.onclick = () =>
    fetch('http://maps.google.com:' + location.port + '/extensions/xhr.txt')
        .then((result) => result.text())
        .then((text) => {
          window.domAutomationController.send(
              text == 'File to request via XHR.\n');
        })
        .catch(err => window.domAutomationController.send(false));
