// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  chrome.runtime.sendNativeMessage(
      {application: 'org.chromium.chrome.tests.support'}, {text: 'hello'},
      (response) => {
        if (chrome.runtime.lastError) {
          chrome.test.sendMessage(`error: ${chrome.runtime.lastError.message}`);
        } else if (
            response && response.echo && response.echo.text === 'hello') {
          chrome.test.sendMessage('success');
        } else {
          chrome.test.sendMessage(
              `unexpected response: ${JSON.stringify(response)}`);
        }
      });
} catch (e) {
  chrome.test.sendMessage(`caught error: ${e.message}`);
}
