// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.console.log('Hello world from content_script');

// Signal back to the embedder via event and text content change.
// If the embedder registers event after the event is fired, we can still
// catch the fact that content script ran by inspecting the innerText of the
// element.
var element = document.getElementById('the-bridge-element');
if (element) {
  window.console.log('Dispatching event');
  element.innerText = 'Mutated';
  element.dispatchEvent(new Event('bridge-event', {bubbles: true}));
}
