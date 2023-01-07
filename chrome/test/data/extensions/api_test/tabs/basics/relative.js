// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.extension.getViews({type:"tab"}).forEach(function(view) {
    if (view.relativePageLoaded) {
      view.relativePageLoaded();
    }
  });
}
