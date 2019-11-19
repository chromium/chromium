// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function contains_all(obj, keys) {
  for (var i = 0; i < keys.length; ++i) {
    if (!obj[keys[i]])
      return false;
  }
  return true;
}

var contents = [
  'download', 'search', 'pause', 'resume', 'cancel', 'getFileIcon', 'open',
  'show', 'erase', 'acceptDanger', 'onCreated', 'onChanged', 'onErased',
  'onDeterminingFilename'];

if (!chrome.downloads ||
    !contains_all(chrome.downloads, contents)) {
  chrome.test.fail();
} else {
  chrome.test.succeed();
}
