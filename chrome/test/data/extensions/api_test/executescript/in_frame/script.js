// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getStyle(elem, name) {
  if (document.defaultView && document.defaultView.getComputedStyle) {
    name = name.toLowerCase();

    try {
      var s = document.defaultView.getComputedStyle(elem, '');
      return s && s.getPropertyValue(name);
    } catch (ex) {
      return null;
    }
  } else {
    return null;
  }
}

var bElement = document.getElementById('test2');
var display = getStyle(bElement, 'display').toLowerCase();
var extensionPort = chrome.runtime.connect();
extensionPort.postMessage({message: display});
