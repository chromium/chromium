// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export {};

const tests = [
  // Verifies that opening save file picker is not blocked.
  // This test should ideally perform an end to end PDF save. There is a
  // pre-existing test to do that, but it's disabled due to flakiness that is
  // rooted in UI interaction.
  // TODO(crbug.com/40803991): Consider removing this test once
  // `PDFExtensionSaveTest.Save` is re-enabled.
  async function testShowFilePicker() {
    await window.showSaveFilePicker({
      suggestedName: 'temp.pdf',
      types: [{
        description: 'PDF Files',
        accept: {'application/pdf': ['.pdf']},
      }],
    });
    chrome.test.succeed();
  },

  // TODO(crbug.com/382610226): Add a test to verify user selection can be
  // retrieved when there are more than one PDF save types.
];

chrome.test.runTests(tests);
