// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kPdfMimeType = 'application/pdf';

function getOptions() {
  return chrome.mimeHandler.getMimeHandlerOptions(kPdfMimeType);
}

// The manifest values are the opposite of the defaults, so each check after a
// reset also fails if the reset restores the defaults instead.
chrome.test.runTests([
  // A null member should bring back the manifest value of that option only.
  async function nullRestoresManifestValueOfOneOption() {
    await chrome.mimeHandler.setMimeHandlerOptions(
        kPdfMimeType, {enabled: true, canEmbed: false});
    chrome.test.assertEq({enabled: true, canEmbed: false}, await getOptions());

    await chrome.mimeHandler.setMimeHandlerOptions(
        kPdfMimeType, {enabled: null});
    chrome.test.assertEq({enabled: false, canEmbed: false}, await getOptions());

    chrome.test.succeed();
  },

  // Null for every member should bring back all manifest values.
  async function nullRestoresManifestValuesOfAllOptions() {
    await chrome.mimeHandler.setMimeHandlerOptions(
        kPdfMimeType, {enabled: true, canEmbed: false});
    chrome.test.assertEq({enabled: true, canEmbed: false}, await getOptions());

    await chrome.mimeHandler.setMimeHandlerOptions(
        kPdfMimeType, {enabled: null, canEmbed: null});
    chrome.test.assertEq({enabled: false, canEmbed: true}, await getOptions());

    chrome.test.succeed();
  },
]);
