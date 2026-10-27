// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export async function testWebGLAndWebGPU(testRunner, description) {
  const {session} = await testRunner.startBlank(description);

  const result = await session.evaluateAsync(async () => {
    const canvas = document.createElement('canvas');
    const context = canvas.getContext('webgl');
    const webglStatus = !context ?
        'null' :
        (context instanceof WebGLRenderingContext ? 'WebGLRenderingContext' :
                                                    'unexpected');

    const adapter = await navigator.gpu.requestAdapter();
    const webgpuStatus = !adapter ?
        'null' :
        (adapter instanceof GPUAdapter ? 'GPUAdapter' : 'unexpected');

    return `WebGL context: ${webglStatus}\nWebGPU adapter: ${webgpuStatus}`;
  });

  testRunner.log(result);

  testRunner.completeTest();
}
