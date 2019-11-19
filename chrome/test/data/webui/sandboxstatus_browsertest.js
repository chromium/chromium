// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

};

// This test is for Linux only.
// PLEASE READ:
// - If failures of this test are a problem on a bot under your care,
//   the proper way to address such failures is to install the SUID
//   sandbox. See:
//     https://chromium.googlesource.com/chromium/src/+/master/docs/linux_suid_sandbox_development.md
// - PLEASE DO NOT GLOBALLY DISABLE THIS TEST.
GEN('#if defined(OS_LINUX)');
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
    function() {
      var sandboxnamespacestring = 'Layer 1 Sandbox\tNamespace';
      var sandboxsuidstring = 'Layer 1 Sandbox\tSUID';

      var namespaceyes = document.body.innerText.match(sandboxnamespacestring);
      var suidyes = document.body.innerText.match(sandboxsuidstring);

      // Exactly one of the namespace or suid sandbox should be enabled.
      expectTrue(suidyes !== null || namespaceyes !== null);
      expectFalse(suidyes !== null && namespaceyes !== null);
    });

// The seccomp-bpf sandbox is also not compatible with ASAN.
GEN('#if !defined(OS_LINUX)');
GEN('# define MAYBE_testBPFSandboxEnabled \\');
GEN('     DISABLED_testBPFSandboxEnabled');
GEN('#else');
GEN('# define MAYBE_testBPFSandboxEnabled \\');
GEN('     testBPFSandboxEnabled');
GEN('#endif');

/**
 * Test if the seccomp-bpf sandbox is enabled.
 */
TEST_F('SandboxStatusUITest', 'MAYBE_testBPFSandboxEnabled', function() {
  var bpfyesstring = 'Seccomp-BPF sandbox\tYes';
  var bpfnostring = 'Seccomp-BPF sandbox\tNo';
  var bpfyes = document.body.innerText.match(bpfyesstring);
  var bpfno = document.body.innerText.match(bpfnostring);

  expectEquals(null, bpfno);
  assertFalse(bpfyes === null);
  expectEquals(bpfyesstring, bpfyes[0]);
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
};

// This test is disabled because it can only pass on real hardware. We
// arrange for it to run on real hardware in specific configurations
// (such as Chrome OS hardware, via Autotest), then run it with
// --gtest_also_run_disabled_tests on those configurations.

/**
 * Test if the GPU sandbox is enabled.
 */
TEST_F('GPUSandboxStatusUITest', 'DISABLED_testGPUSandboxEnabled', function() {
  const gpuyesstring = 'Sandboxed\ttrue';
  const gpunostring = 'Sandboxed\tfalse';

  let observer = new MutationObserver(function(mutations) {
    mutations.forEach(function(mutation) {
      for (var i = 0; i < mutation.addedNodes.length; i++) {
        // Here we can inspect each of the added nodes. We expect
        // to find one that contains one of the GPU status strings.
        let addedNode = mutation.addedNodes[i];
        // Check for both. If it contains neither, it's an unrelated
        // mutation event we don't care about. But if it contains one,
        // pass or fail accordingly.
        let gpuyes = addedNode.innerText.match(gpuyesstring);
        let gpuno = addedNode.innerText.match(gpunostring);
        if (gpuyes || gpuno) {
          expectEquals(null, gpuno);
          expectTrue(gpuyes && (gpuyes[0] == gpuyesstring));
          testDone();
        }
      }
    });
  });
  observer.observe(document.getElementById('basic-info'), {childList: true});
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
  isAsync: true
};

// This test is for Windows only.
GEN('#if defined(OS_WIN)');
GEN('# define MAYBE_testSandboxStatus \\');
GEN('     testSandboxStatus');
GEN('#else');
GEN('# define MAYBE_testSandboxStatus \\');
GEN('     DISABLED_testSandboxStatus');
GEN('#endif');

/**
 * Test that chrome://sandbox functions on Windows.
 */
TEST_F('SandboxStatusWindowsUITest', 'MAYBE_testSandboxStatus', function() {
  var sandboxTitle = 'Sandbox Status';
  var sandboxPolicies = 'policies:';
  var sandboxMitigations = 'platformMitigations';

  var titleyes = document.body.innerText.match(sandboxTitle);
  expectTrue(titleyes !== null);

  var rawNode = document.getElementById('raw-info');
  var policiesyes = rawNode.innerText.match(sandboxPolicies);
  expectTrue(policiesyes !== null);
  var mitigationsyes = rawNode.innerText.match(sandboxMitigations);
  expectTrue(mitigationsyes !== null);

  testDone();
});
