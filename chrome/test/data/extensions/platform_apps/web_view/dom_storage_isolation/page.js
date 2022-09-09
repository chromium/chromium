// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function initDOMStorage(name) {
  window.localStorage.setItem('foo', 'local-' + name);
  window.sessionStorage.setItem('baz', 'session-' + name);
}

function getLocalStorage() {
  return window.localStorage.getItem('foo') || 'badval';
}

function getSessionStorage() {
  return window.sessionStorage.getItem('baz') || 'badval';
}

function initialize() {
  var messageHandler = new Messaging.Handler();
  messageHandler.addHandler(INIT_DOM_STORAGE, function(message, portFrom) {
    initDOMStorage(message.pageName);
    messageHandler.sendMessage({title: INIT_DOM_STORAGE_COMPLETE}, portFrom);
  });
  messageHandler.addHandler(GET_DOM_STORAGE_INFO, function(message, portFrom) {
    messageHandler.sendMessage(new Messaging.Message(
        GET_DOM_STORAGE_INFO_COMPLETE, {
          local: getLocalStorage(),
          session: getSessionStorage()
        }), portFrom);
  });
}
window.onload = initialize;
