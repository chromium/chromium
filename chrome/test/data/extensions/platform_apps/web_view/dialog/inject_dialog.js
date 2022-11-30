// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;

function reportConnected() {
  var msg = ['connected'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

function reportAlertCompletion(messageText) {
  window.alert(messageText);
  var msg = ['alert-dialog-done'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

function reportConfirmDialogResult(messageText) {
  var result = window.confirm(messageText);
  var msg = ['confirm-dialog-result', result];
  embedder.postMessage(JSON.stringify(msg), '*');
}

function reportPromptDialogResult(messageText, defaultPromptText) {
  var result = window.prompt(messageText, defaultPromptText);
  var msg = ['prompt-dialog-result', result];
  embedder.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  var data = JSON.parse(e.data);
  switch (data[0]) {
    case 'connect': {
      reportConnected();
      break;
    }
    case 'start-confirm-dialog-test': {
      var messageText = data[1];
      reportConfirmDialogResult(messageText);
      break;
    }
    case 'start-alert-dialog-test': {
      var messageText = data[1];
      reportAlertCompletion(messageText);
      break;
    }
    case 'start-prompt-dialog-test': {
      var messageText = data[1];
      var defaultPromptText = data[2];
      reportPromptDialogResult(messageText, defaultPromptText);
      break;
    }
  }
});
