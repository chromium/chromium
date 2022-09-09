// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Send the variable defined by a.js and modified by b.js back to the extension.
chrome.runtime.connect().postMessage(num);
