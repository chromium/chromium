// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function create(title, type, parent, callback) {
  props = {};
  if (title) {
    props.title = title;
  }
  props.type = type;

  if (parent) {
    props.parentId = parent;
  }

  chrome.contextMenus.create(props, function() {
    if (!chrome.runtime.lastError && callback) {
      callback();
    }
  });
}

function createTestSet(parent, callback) {
  create("radio1", "radio", parent);
  create("radio2", "radio", parent);
  create("normal1", "normal", parent);
  create(null, "separator", parent);
  create("normal2", "normal", parent);
  create(null, "separator", parent);
  create("radio3", "radio", parent);
  create("radio4", "radio", parent);
  create(null, "separator", parent);
  create("normal3", "normal", parent, callback);
}
