// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This shouldn't be sent to the error console, because we "handle" the error
// by checking chrome.runtime.lastError.
chrome.permissions.remove({permissions: ['kalman']}, function() {
  var error = chrome.runtime.lastError;
});

// This should be sent to the error console, because lastError is unchecked.
chrome.permissions.remove({permissions: ['foobar']}, function() { });
