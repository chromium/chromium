// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var scriptMessageEvent;
var pageToScriptTunnel;
var scriptToPageTunnel;

function setStatus(status) {
  document.getElementById('status').innerText = status;
}

function setupScriptTunnel() {
  scriptMessageEvent = document.createEvent("Event");
  scriptMessageEvent.initEvent('scriptMessage', true, true);

  pageToScriptTunnel = document.getElementById("pageToScriptTunnel");
  scriptToPageTunnel = document.getElementById("scriptToPageTunnel");

  scriptToPageTunnel.addEventListener("scriptMessage", function() {
    var data = JSON.parse(scriptToPageTunnel.innerText);
    window[data.name](data.args);
  });
}

function messageData(data) {
  var args = [];
  for (var i = 0; i < data.length; i++) {
    args.push(data[i]);
  }
  return {
    'name': data.callee.name.replace(/notify/g, "on"),
    'args': args
  }
}

function notifyBackgroundPageResponded() {
  pageToScriptTunnel.innerText = JSON.stringify(messageData(arguments));
  pageToScriptTunnel.dispatchEvent(scriptMessageEvent);
}

function notifyBackgroundPageLoaded() {
  pageToScriptTunnel.innerText = JSON.stringify(messageData(arguments));
  pageToScriptTunnel.dispatchEvent(scriptMessageEvent);
}

function notifyBackgroundPagePermissionDenied() {
  pageToScriptTunnel.innerText = JSON.stringify(messageData(arguments));
  pageToScriptTunnel.dispatchEvent(scriptMessageEvent);
}

function notifyCounterError() {
  pageToScriptTunnel.innerText = JSON.stringify(messageData(arguments));
  pageToScriptTunnel.dispatchEvent(scriptMessageEvent);
}

function notifyBackgroundPageClosing() {
  pageToScriptTunnel.innerText = JSON.stringify(messageData(arguments));
  pageToScriptTunnel.dispatchEvent(scriptMessageEvent);
}

function notifyBackgroundPageClosed() {
  pageToScriptTunnel.innerText = JSON.stringify(messageData(arguments));
  pageToScriptTunnel.dispatchEvent(scriptMessageEvent);
}
