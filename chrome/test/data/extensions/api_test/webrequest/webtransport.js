// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function expectSessionEstablished(url) {
  const transport = new WebTransport(url);
  try {
    await transport.ready;
  } catch (e) {
    chrome.test.fail('Ready rejected: ${e}');
  }
}

async function expectSessionFailed(url) {
  const transport = new WebTransport(url);
  try {
    await transport.ready;
    chrome.test.fail('Ready should be rejected.');
  } catch (e) {
    // TODO(crbug.com/1240935): Consider showing error.
    // This is filtered by InterceptingHandshakeClient.
    chrome.test.assertEq({}, e);
  }
}
