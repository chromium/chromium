// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function create(createProperties) {
  return new Promise(resolve => {
    chrome.contextMenus.create(createProperties, function() {
      var error = !!chrome.runtime.lastError;
      resolve(error);
    });
  });
}

function update(id, updateProperties) {
  return new Promise(resolve => {
    chrome.contextMenus.update(id, updateProperties, function() {
      var error = !!chrome.runtime.lastError;
      resolve(error);
    });
  });
}
