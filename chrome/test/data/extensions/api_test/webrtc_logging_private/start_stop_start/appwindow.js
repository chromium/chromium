// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Making it explicit that this is exposed for the background page to call.
window.attemptStartStopLogging = function(succeed, fail) {
  chrome.webrtcLoggingPrivate.start({targetWebview: true}, "",
    function() {
      if (chrome.runtime.lastError) {
        fail('start: ' + chrome.runtime.lastError.message);
        return;
      }
      chrome.webrtcLoggingPrivate.stop({targetWebview: true}, "",
        function() {
          if (chrome.runtime.lastError) {
            fail('stop: ' + chrome.runtime.lastError.message);
            return;
          }
          chrome.webrtcLoggingPrivate.discard({targetWebview: true}, "",
            function() {
              if (chrome.runtime.lastError) {
                fail('discard: ' + chrome.runtime.lastError.message);
                return;
              }
              succeed();
            }
          );
        }
      );
    }
  );
};
