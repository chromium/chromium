// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let embedder = null;

function reportConnected() {
  const msg = ['connected'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

function reportAlertCompletion(messageText) {
  window.alert(messageText);
  const msg = ['alert-dialog-done'];
  embedder.postMessage(JSON.stringify(msg), '*');
}

function reportConfirmDialogResult(messageText) {
  const result = window.confirm(messageText);
  const msg = ['confirm-dialog-result', result];
  embedder.postMessage(JSON.stringify(msg), '*');
}

function reportPromptDialogResult(messageText, defaultPromptText) {
  const result = window.prompt(messageText, defaultPromptText);
  const msg = ['prompt-dialog-result', result];
  embedder.postMessage(JSON.stringify(msg), '*');
}

window.addEventListener('message', function(e) {
  embedder = e.source;
  const data = JSON.parse(e.data);
  switch (data[0]) {
    case 'connect': {
      reportConnected();
      break;
    }
    case 'start-confirm-dialog-test': {
      const messageText = data[1];
      reportConfirmDialogResult(messageText);
      break;
    }
    case 'start-alert-dialog-test': {
      const messageText = data[1];
      reportAlertCompletion(messageText);
      break;
    }
    case 'start-prompt-dialog-test': {
      const messageText = data[1];
      const defaultPromptText = data[2];
      reportPromptDialogResult(messageText, defaultPromptText);
      break;
    }
    default: {
      console.error('Unknown message type: ' + data[0]);
      break;
    }
  }
});
