// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use onload event to make sure that the messages are first dispatched in
// frames, and then in the top-level frame. This requires document_start or
// document_end.
window.addEventListener('load', function() {
  chrome.runtime.sendMessage(window.parent === window ? 'parent' : 'child');
});
