// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setWindowName(name) {
  window.name = name;
}

function findWindowByName(name) {
  var w = window.open('', name);
  var found = w.location.href != "about:blank";
  if (!found)
    w.close();
  return found;
}

function initialize() {
  var messageHandler = new Messaging.Handler();
  messageHandler.addHandler(SET_WINDOW_NAME, function(message, portFrom) {
    setWindowName(message.windowName);
    messageHandler.sendMessage({title: SET_WINDOW_NAME_COMPLETE}, portFrom);
  });
  messageHandler.addHandler(FIND_WINDOW_BY_NAME, function(message, portFrom) {
    var found = findWindowByName(message.windowName);
    messageHandler.sendMessage(
        {title: FIND_WINDOW_BY_NAME_COMPLETE, found: found},
        portFrom);
  });
}
window.onload = initialize;
