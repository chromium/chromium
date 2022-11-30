// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  window.onunload = function() {
    window.parent.postMessage({'success': false,
                               'reason' : 'unload handler works'},
                              '*');
  };
  if (typeof(window.unload) !== 'undefined') {
    window.parent.postMessage({'success': false,
                               'reason' : 'unload is not undefined'},
                              '*');
  }
  window.dispatchEvent(new Event('unload'));
  window.parent.postMessage({'success': true}, '*');
};
