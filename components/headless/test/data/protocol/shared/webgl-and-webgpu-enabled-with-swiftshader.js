// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WebGL and WebGPU in headless mode require --enable-unsafe-swiftshader (and
// --enable-unsafe-webgpu for WebGPU's SwiftShader CPU fallback adapter).
// META: --enable-unsafe-swiftshader
// META: --enable-unsafe-webgpu

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {testWebGLAndWebGPU} =
      await import('/protocol/shared/webgl-and-webgpu-helper.js');
  await testWebGLAndWebGPU(
      testRunner,
      'Tests that WebGL and WebGPU are enabled with ' +
          '--enable-unsafe-swiftshader and --enable-unsafe-webgpu.');
});
