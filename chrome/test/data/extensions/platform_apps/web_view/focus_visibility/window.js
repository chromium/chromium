// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
'use strict';

var isOopif = false;
var webViewButtonFocused = false;

var webviewReplyCallback = null;

function listenForForKeyupAndButtonFocus() {
  window.addEventListener('keyup', function() {
    // With BrowserPlugin <webview> this renderer might see all events targetted
    // to the <webview> before ultimately forwarding to the <webview>. Suppress
    // the events once the webview with active.
    // OTOH in OOPIF-<webiew>, keyup is only ever dispatched to one renderer.
    // (The keyup that changes focus to the <webview> is sent to the embedder
    // but activeElement is the <webview> by that point.)
    if (isOopif || document.activeElement !== getWebView()) {
      chrome.test.sendMessage('WebViewInteractiveTest.KeyUp');
    }
  });
  window.addEventListener('message', function(e) {
    if (e.data === 'guest-keyup') {
      chrome.test.sendMessage('WebViewInteractiveTest.KeyUp');
    }
    else if (e.data === 'focus-event') {
      webViewButtonFocused = true;
    } else {
      if (webviewReplyCallback) {
        var temp = webviewReplyCallback;
        webviewReplyCallback = null;
        temp(e.data);
      }
    }
  });
}

function setUpWebView(embedder) {
  var webview = document.createElement('webview');
  embedder.appendChild(webview);
  chrome.test.getConfig(function(config) {
    var url = 'http://localhost:' + config.testServer.port
        + '/extensions/platform_apps/web_view/focus_visibility/guest.html';
    webview.onloadstop = function() {
      function callback(e) {
        if (e.data === 'connected') {
          e.stopImmediatePropagation();
          window.removeEventListener('message', callback);
          chrome.test.sendMessage('WebViewInteractiveTest.WebViewInitialized');
        }
      };
      window.addEventListener('message', callback);
      getWebView().contentWindow.postMessage('connect', '*');
    };
    webview.src = url;
    console.log('Setting URL to "' + url + '".');
  });
}

function reset() {
  getWebView().style.visibility = 'visible';
  document.querySelector('#before').focus();
}

function sendMessageToWebViewAndReceiveReply(message, replyCallback) {
  if (replyCallback) {
    webviewReplyCallback = replyCallback;
  }
  getWebView().contentWindow.postMessage(message, '*');
}

function getWebView() {
  return document.querySelector('webview');
}

window.onAppMessage = function(command) {
  switch (command) {
    case 'init-oopif':
      isOopif = true;
      // fallthrough
    case 'init':
      listenForForKeyupAndButtonFocus();
      document.querySelector('#before').focus();
      setUpWebView(document.querySelector('div'));
      break;
    case 'reset':
      reset();
      sendMessageToWebViewAndReceiveReply("reset", function(reply) {
        if (reply === 'reset-complete') {
          webViewButtonFocused = false;
          chrome.test.sendMessage('WebViewInteractiveTest.DidReset');
        }
      });
      break;
    case 'verify':
      chrome.test.sendMessage(webViewButtonFocused ?
          'WebViewInteractiveTest.WebViewButtonWasFocused' :
          'WebViewInteractiveTest.WebViewButtonWasNotFocused');
      break;
    case 'hide-webview':
      getWebView().style.visibility = 'hidden';
      chrome.test.sendMessage('WebViewInteractiveTest.DidHideWebView');
      break;
  }
}
window.onload = function() {
  chrome.test.sendMessage('WebViewInteractiveTest.LOADED');
}
