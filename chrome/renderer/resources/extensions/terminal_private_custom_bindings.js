// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for chrome.terminalPrivate API.
bindingUtil.registerEventArgumentMassager(
    'terminalPrivate.onProcessOutput', function(args, dispatch) {
      const terminalId = args[0];
      try {
        dispatch(args);
      } finally {
        chrome.terminalPrivate.ackOutput(terminalId);
      }
    });
