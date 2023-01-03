// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add a button to the page that can fetch resource.
const button = document.createElement('button');
button.id = 'fetchResourceButton';
button.innerText = 'fetch resource';
document.body.appendChild(button);

// This fetch runs in the isolated world.
button.onclick = async () => {
  await fetch('/extensions/resource_timing/24.png').then(
    () => { window.domAutomationController.send(true); }).catch(
      () => { window.domAutomationController.send(false); });
};

// This is loaded as a script tag element into the DOM and the fetch runs in the
// main world.
async function fetchResource() {
  await fetch('/extensions/resource_timing/24.png');
}
