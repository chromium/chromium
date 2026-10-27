// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {testWebGLAndWebGPU} =
      await import('/protocol/shared/webgl-and-webgpu-helper.js');
  await testWebGLAndWebGPU(
      testRunner, 'Tests that WebGL and WebGPU are disabled by default.');
});
