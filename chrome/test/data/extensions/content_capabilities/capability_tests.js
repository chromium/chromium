// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

var getIframe = function() { return document.querySelector('iframe'); };

window.tests = {
  canReadClipboard: function() {
    domAutomationController.send(document.execCommand('paste'));
  },

  canWriteClipboard: function() {
    domAutomationController.send(document.execCommand('copy'));
  },

  canReadClipboardInAboutBlankFrame: function() {
    domAutomationController.send(
        getIframe().contentDocument.execCommand('paste'));
  },

  canWriteClipboardInAboutBlankFrame: function() {
    domAutomationController.send(
        getIframe().contentDocument.execCommand('copy'));
  },
};

}());
