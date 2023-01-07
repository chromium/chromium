// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var writeSuccess = true;
try {
  Document.prototype.write.call(document, 'Hello, world');
} catch (e) {
  writeSuccess = false;
}

if (writeSuccess)
  chrome.test.fail();
else
  chrome.test.succeed();
