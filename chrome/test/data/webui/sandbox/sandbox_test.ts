// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('Sandbox', () => {
  test('SUIDorNamespaceSandboxEnabled', () => {
    const sandboxnamespacestring = 'Layer 1 Sandbox\tNamespace';
    const sandboxsuidstring = 'Layer 1 Sandbox\tSUID';

    const namespaceyes = document.body.innerText.match(sandboxnamespacestring);
    const suidyes = document.body.innerText.match(sandboxsuidstring);

    // Exactly one of the namespace or suid sandbox should be enabled.
    assertTrue(suidyes !== null || namespaceyes !== null);
    assertFalse(suidyes !== null && namespaceyes !== null);
  });

  test('BPFSandboxEnabled', () => {
    const bpfyesstring = 'Seccomp-BPF sandbox\tYes';
    const bpfnostring = 'Seccomp-BPF sandbox\tNo';
    const bpfyes = document.body.innerText.match(bpfyesstring);
    const bpfno = document.body.innerText.match(bpfnostring);

    assertEquals(null, bpfno);
    assertTrue(!!bpfyes);
    assertEquals(bpfyesstring, bpfyes[0]);
  });

  test('SandboxStatus', () => {
    const sandboxTitle = 'Sandbox Status';
    const sandboxPolicies = 'policies:';
    const sandboxMitigations = 'platformMitigations';

    const titleyes = document.body.innerText.match(sandboxTitle);
    assertTrue(titleyes !== null);

    const rawNode = document.getElementById('raw-info');
    assertTrue(!!rawNode);
    const policiesyes = rawNode.innerText.match(sandboxPolicies);
    assertTrue(policiesyes !== null);
    const mitigationsyes = rawNode.innerText.match(sandboxMitigations);
    assertTrue(mitigationsyes !== null);
  });
});
