// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Attempts to fetch and instantiate a simple Wasm module.
async function instantiateFetch() {
  const bytes = await fetch("empty.wasm");

  try {
    const instance = await WebAssembly.instantiateStreaming(bytes);
    domAutomationController.send("success");
  } catch (e) {
    domAutomationController.send("failure");
  }
}

// Attempts to instantiate a simple Wasm module.
async function instantiateArrayBuffer() {
  // The smallest possible Wasm module. Just the header (0, "A", "S", "M"), and
  // the version (0x1).
  const bytes = new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0]);

  try {
    const instance = await WebAssembly.instantiate(bytes);
    domAutomationController.send("success");
  } catch (e) {
    domAutomationController.send("failure");
  }
}
