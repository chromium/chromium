// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add a button to the page that can add a cross-origin iframe.
const button = document.createElement('button');
button.id = 'addIframeButton';
button.innerText = 'Add Iframe';
document.body.appendChild(button);

// Adds an iframe. Notifies the test if it succeeds or fails.
button.onclick = () => {
  const frame = document.createElement('iframe');
  frame.name = 'added-by-extension';
  frame.src = `http://cross-origin.com:${location.port}` +
      '/extensions/csp/success.html';
  frame.onload = () => { window.domAutomationController.send(true); };
  document.body.appendChild(frame);
};
