// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupMessageHandler() {
  window.onmessage = function(e) {
    if (e.data.command === 'start-download') {
      console.log("Start Download received for URL " + e.data.url);
      var anchor = document.createElement('a');
      anchor.href = e.data.url;
      anchor.download = 'foo';
      document.getElementById('container').appendChild(anchor);
      anchor.click();
    }
  };
}

function setCookiesBasedOnURLFragment() {
  var cookieValue = window.location.hash.substr(1);
  var expiresOn = new Date();
  expiresOn.setMonth(expiresOn.getMonth() + 1);
  cookieValue += ";path=/";
  cookieValue += ";expires=" + expiresOn.toUTCString();
  document.cookie = cookieValue;
  console.log("Setting cookie: " + cookieValue);
}

window.onload = function() {
  setCookiesBasedOnURLFragment();
  setupMessageHandler();
}
