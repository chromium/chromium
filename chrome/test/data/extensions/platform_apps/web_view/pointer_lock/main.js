// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See chrome/browser/extensions/web_view_interactive_browsertest.cc
// (WebViewInteractiveTest, PointerLock) for documentation on this test.
var requestCount = 0;
var guestURL;
var startTest = function() {
  window.addEventListener('message', receiveMessage, false);
  chrome.test.sendMessage('guest-loaded');
  var webview = document.getElementById('webview');
  webview.addEventListener('loadstop', function(e) {
    webview.contentWindow.postMessage('msg', '*');
  });
  webview.addEventListener('permissionrequest', function(e) {
    if (requestCount == 0) {
      setTimeout(function() {
        try {
          e.request.allow();
        } catch (exception) {
          chrome.test.sendMessage('request exception');
        }
      }, 10);
    } else if (requestCount == 1) {
      e.request.deny();
    } else if (requestCount == 2) {
      if (e.permission == 'pointerLock' && !e.lastUnlockedBySelf &&
          e.url == guestURL && e.userGesture) {
        e.preventDefault();
        setTimeout(function() { e.request.allow(); }, 0);
      } else {
        if (e.permission != 'pointerLock') {
          console.log('Permission was, "' + e.permission + '" when it ' +
                      'should have been "pointerLock"');
        }
        if (e.lastUnlockedBySelf) {
          console.log('e.lastUnlockedBySelf should have been false.');
        }
        if (e.url == guestURL) {
          console.log('e.url was, "' + e.url + '" when it ' +
                      'should have been "' + guestURL + '"');
        }
        if (!e.userGesture) {
          console.log('e.userGesture should have been true.');
        }
      }
    } else if (requestCount == 3) {
      if (e.permission == 'pointerLock' && e.url == guestURL && e.userGesture) {
        e.request.allow();
      } else {
        if (e.permission != 'pointerLock') {
          console.log('Permission was, "' + e.permission + '" when it ' +
                      'should have been "pointerLock"');
        }
        if (e.url == guestURL) {
          console.log('e.url was, "' + e.url + '" when it ' +
                      'should have been "' + guestURL + '"');
        }
        if (!e.userGesture) {
          console.log('e.userGesture should have been true.');
        }
      }
    }
    requestCount++;
  });
};
var fail = false;
var receiveMessage = function(event) {
  if (event.data == 'Pointer was not locked to locktarget1.') {
    fail = true;
    chrome.test.fail(event.data);
  } else if (event.data == 'delete me') {
    var webview_container = document.getElementById('webview-tag-container');
    webview_container.parentNode.removeChild(webview_container);
    setTimeout(function() { chrome.test.sendMessage('timeout'); }, 5000);
    document.getElementById('mousemove-capture-container').addEventListener(
        'mousemove', function (e) {
      console.log('move-captured');
      chrome.test.sendMessage('move-captured');
    });
    console.log('deleted');
    chrome.test.sendMessage('deleted');
  }
  if (!fail) {
    chrome.test.sendMessage(event.data);
  }
}

chrome.test.getConfig(function(config) {
  guestURL = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/pointer_lock/guest.html';
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview id=\'webview\' style="width: 400px; height: 400px; ' +
      'margin: 0; padding: 0;"' +
      ' src="' + guestURL + '"' +
      '></webview>';
  startTest();
});
