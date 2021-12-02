// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/access_code_cast.js';

suite('AccessCodeCastAppTest', () => {
  /** @type {!AccessCodeCastElement} */
  let app;

  setup(() => {
    PolymerTest.clearBody();

    app = document.createElement('access-code-cast-app');
    document.body.appendChild(app);
  });

  test('codeInputView is shown and qrInputView is hidded by default', () => {
    assertFalse(app.$.codeInputView.hidden);
    assertTrue(app.$.qrInputView.hidden);
  });

  test('the "switchToQrInput" function switches the view correctly', () => {
    app.switchToQrInput();

    assertTrue(app.$.codeInputView.hidden);
    assertFalse(app.$.qrInputView.hidden);
  });

  test('the "switchToCodeInput" function switches the view correctly', () => {
    // Start on the qr input view and check we are really there
    app.switchToQrInput();
    assertTrue(app.$.codeInputView.hidden);
    assertFalse(app.$.qrInputView.hidden);

    app.switchToCodeInput();

    assertFalse(app.$.codeInputView.hidden);
    assertTrue(app.$.qrInputView.hidden);
  });
});
