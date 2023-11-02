// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(var_args) {
  window.console.log.apply(window.console, arguments);
};

var initialize = function() {
  var fullscreenElement = document.createElement('div');
  fullscreenElement.innerText = 'Test fullscreen element';
  document.body.appendChild(fullscreenElement);

  document.onclick = function(e) {
    LOG('document.click, requesting fullscreen');
    fullscreenElement.webkitRequestFullScreen();
  };

  document.onwebkitfullscreenchange = function() {
    var hasFullscreenElement = !!document.webkitFullscreenElement;
    var isFullscreen = document.webkitIsFullScreen;
    if (hasFullscreenElement != isFullscreen) {
      LOG('STATUS{"isFullscreenChange": 1, "failed": true}');
    } else {
      if (isFullscreen) {
        LOG('STATUS{"isFullscreenChange": 1, "changeType": "enter"}');
      } else {
        LOG('STATUS{"isFullscreenChange": 1, "changeType": "exit"}');
      }
    }
  };

  document.onwebkitfullscreenerror = function() {
    LOG('STATUS{"isFullscreenChange": 1, "changeType": "exit"}');
  };
};

initialize();
