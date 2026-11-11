// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SignatureSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {getRequiredElement} from './test_util.js';

suite('Ink2SignatureSelector', function() {
  let signatureSelector: SignatureSelectorElement;

  setup(function() {
    document.body.innerHTML = '';
    signatureSelector = document.createElement('signature-selector');
    document.body.appendChild(signatureSelector);
  });

  // Test clicking the new signature button dispatches a new-signature-click
  // event.
  test('NewSignature', async function() {
    const newSignatureButton =
        getRequiredElement(signatureSelector, '#new-signature');
    const whenNewSignature =
        eventToPromise('new-signature-click', signatureSelector);
    newSignatureButton.click();
    await whenNewSignature;
  });
});
