// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "build/build_config.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * TestFixture for SUID Sandbox testing.
 * @extends {testing.Test}
 * @constructor
 */
function SandboxStatusUITest() {}

SandboxStatusUITest.prototype = {
  __proto__: testing.Test.prototype,
  /**
   * Browse to the options page & call our preLoad().
   */
  browsePreload: 'chrome://sandbox',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

// This test is for Linux only.
// PLEASE READ:
// - If failures of this test are a problem on a bot under your care,
//   the proper way to address such failures is to install the SUID
//   sandbox. See:
//     https://chromium.googlesource.com/chromium/src/+/main/docs/linux/suid_sandbox_development.md
// - PLEASE DO NOT GLOBALLY DISABLE THIS TEST.
GEN('#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)');
GEN('# define MAYBE_testSUIDorNamespaceSandboxEnabled \\');
GEN('     testSUIDorNamespaceSandboxEnabled');
GEN('#else');
GEN('# define MAYBE_testSUIDorNamespaceSandboxEnabled \\');
GEN('     DISABLED_testSUIDorNamespaceSandboxEnabled');
GEN('#endif');

/**
 * Test if the SUID sandbox is enabled.
 */
TEST_F(
    'SandboxStatusUITest', 'MAYBE_testSUIDorNamespaceSandboxEnabled',
    async function() {
      const module = await import('chrome://resources/js/static_types.js');
      const script = document.createElement('script');
      script.type = 'module';
      script.onload = () => {
        runMochaTest('Sandbox', 'SUIDorNamespaceSandboxEnabled');
      };
      script.src =
          module
              .getTrustedScriptURL`chrome://webui-test/sandbox/sandbox_test.js`;
      document.body.appendChild(script);
    });

// The seccomp-bpf sandbox is also not compatible with ASAN.
GEN('#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)');
GEN('# define MAYBE_testBPFSandboxEnabled \\');
GEN('     DISABLED_testBPFSandboxEnabled');
GEN('#else');
GEN('# define MAYBE_testBPFSandboxEnabled \\');
GEN('     testBPFSandboxEnabled');
GEN('#endif');

/**
 * Test if the seccomp-bpf sandbox is enabled.
 */
TEST_F('SandboxStatusUITest', 'MAYBE_testBPFSandboxEnabled', async function() {
  const module = await import('chrome://resources/js/static_types.js');
  const script = document.createElement('script');
  script.type = 'module';
  script.onload = () => {
    runMochaTest('Sandbox', 'BPFSandboxEnabled');
  };
  script.src =
      module.getTrustedScriptURL`chrome://webui-test/sandbox/sandbox_test.js`;
  document.body.appendChild(script);
});

/**
 * TestFixture for GPU Sandbox testing.
 * @extends {testing.Test}
 * @constructor
 */
function GPUSandboxStatusUITest() {}

GPUSandboxStatusUITest.prototype = {
  __proto__: testing.Test.prototype,
  /**
   * Browse to the options page & call our preLoad().
   */
  browsePreload: 'chrome://gpu',
  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

// This test is disabled because it can only pass on real hardware. We
// arrange for it to run on real hardware in specific configurations
// (such as Chrome OS hardware, via Autotest), then run it with
// --gtest_also_run_disabled_tests on those configurations.

/**
 * Test if the GPU sandbox is enabled.
 */
TEST_F(
    'GPUSandboxStatusUITest', 'DISABLED_testGPUSandboxEnabled',
    async function() {
      const module = await import('chrome://resources/js/static_types.js');
      const script = document.createElement('script');
      script.type = 'module';
      script.onload = () => {
        runMochaTest('GPU', 'GPUSandboxEnabled');
      };
      script.src =
          module.getTrustedScriptURL`chrome://webui-test/sandbox/gpu_test.js`;
      document.body.appendChild(script);
    });

/**
 * TestFixture for chrome://sandbox on Windows.
 * @extends {testing.Test}
 * @constructor
 */
function SandboxStatusWindowsUITest() {}

SandboxStatusWindowsUITest.prototype = {
  __proto__: testing.Test.prototype,
  /**
   * Browse to the options page & call our preLoad().
   */
  browsePreload: 'chrome://sandbox',
  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

// This test is for Windows only.
GEN('#if BUILDFLAG(IS_WIN)');
GEN('# define MAYBE_testSandboxStatus \\');
// TODO(https://crbug.com/1045564) Flaky on Windows.
GEN('     DISABLED_testSandboxStatus');
GEN('#else');
GEN('# define MAYBE_testSandboxStatus \\');
GEN('     DISABLED_testSandboxStatus');
GEN('#endif');

/**
 * Test that chrome://sandbox functions on Windows.
 */
TEST_F(
    'SandboxStatusWindowsUITest', 'MAYBE_testSandboxStatus', async function() {
      const module = await import('chrome://resources/js/static_types.js');
      const script = document.createElement('script');
      script.type = 'module';
      script.onload = () => {
        runMochaTest('Sandbox', 'SandboxStatus');
      };
      script.src =
          module
              .getTrustedScriptURL`chrome://webui-test/sandbox/sandbox_test.js`;
      document.body.appendChild(script);
    });
