// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export async function runTests() {

  chrome.test.runTests([
    // Attempts to fetch and instantiate a simple Wasm module.
    async function instantiateFetch() {
      const response = await fetch('empty.wasm');

      let wasmAllowed;
      try {
        const instance = await WebAssembly.instantiateStreaming(response);
        wasmAllowed = true;
      } catch (e) {
        wasmAllowed = false;
      }
      chrome.test.assertFalse(wasmAllowed);
      chrome.test.succeed();
    },

    // Attempts to instantiate a simple Wasm module.
    async function instantiateArrayBuffer() {
      // The smallest possible Wasm module. Just the header (0, "A", "S", "M"),
      // and the version (0x1).
      const bytes = new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0]);

      let wasmAllowed;
      try {
        const instance = await WebAssembly.instantiate(bytes);
        wasmAllowed = true;
      } catch (e) {
        wasmAllowed = false;
      }

      chrome.test.assertFalse(wasmAllowed);
      chrome.test.succeed();
    }
  ]);
}
