// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(config => {
  const {tabId, expectedError, nodeAccess, devtools, pseudoNodeId} =
      JSON.parse(config.customArg);
  const capture = options =>
      chrome.experimentalAiData.getApcSnapshot(tabId, options);

  // Node helpers belong only to content scripts, even for the debugging
  // extension. Background and DevTools pages keep the capture API instead.
  chrome.test.assertEq(undefined, chrome.experimentalAiData.getDomNodeId);
  chrome.test.assertEq(
      undefined, chrome.experimentalAiData.getNodeForDomNodeId);

  function verifyNodes(expectedError, pseudoNodeId) {
    const api = chrome.experimentalAiData;
    const check = (ok, message) => {
      if (!ok) {
        throw new Error(message);
      }
    };
    if (expectedError === 'API access restricted for this extension.') {
      // Feature allowlisting hides the methods rather than throwing on calls.
      check(!api?.getDomNodeId, 'Node ID helper exposed to other extension');
      check(
          !api?.getNodeForDomNodeId, 'Node lookup exposed to other extension');
      return true;
    }
    check(!!api, 'API missing in isolated world');
    // Capture remains a browser API for privileged extension contexts.
    for (const method
             of ['getAiData', 'getAiDataWithSpecifier', 'getApcSnapshot']) {
      check(!api[method], `${method} exposed to content scripts`);
    }
    if (expectedError) {
      for (const method of ['getDomNodeId', 'getNodeForDomNodeId']) {
        let error;
        try {
          api[method](null);
        } catch (e) {
          error = e.message;
        }
        check(error === expectedError, `Unexpected access error: ${error}`);
      }
      return true;
    }
    if (pseudoNodeId) {
      // Pseudo-elements have IDs, but cannot safely become JavaScript Nodes.
      check(
          api.getNodeForDomNodeId(pseudoNodeId) === null,
          'Pseudo-element ID accepted');
      const bodyId = api.getDomNodeId(document.body);
      const resolved = api.getNodeForDomNodeId([bodyId, pseudoNodeId, bodyId]);
      check(
          resolved[0] === document.body && resolved[1] === null &&
              resolved[2] === document.body,
          'Pseudo-element batch entry accepted');
    }
    const nodes = [
      document.body,
      document.createElement('button'),
      document.createElement('input'),
      document.createTextNode('text'),
      document.createComment('comment'),
    ];
    document.body.append(...nodes.slice(1));
    for (const node of nodes) {
      const id = api.getDomNodeId(node);
      check(Number.isInteger(id) && id > 0, 'Invalid node ID');
      check(api.getNodeForDomNodeId(id) === node, 'Single identity lost');
    }
    const ids = api.getDomNodeId(nodes);
    const resolved = api.getNodeForDomNodeId(ids);
    check(
        ids.length === nodes.length && resolved.length === nodes.length,
        'Bulk length mismatch');
    nodes.forEach((node, i) => {
      check(ids[i] === api.getDomNodeId(node), 'Bulk ID mismatch');
      check(resolved[i] === node, 'Bulk identity lost');
    });
    // Invalid entries preserve their positions, including array holes.
    const invalid =
        [null, undefined, 0, -1, 1.5, NaN, Infinity, '1', {}, 2147483648];
    for (const value of invalid) {
      check(api.getDomNodeId(value) === null, 'Non-node accepted');
      check(api.getNodeForDomNodeId(value) === null, 'Invalid ID accepted');
    }
    check(api.getNodeForDomNodeId(2147483647) === null, 'Unknown ID accepted');
    check(api.getDomNodeId([]).length === 0, 'Empty array failed');
    check(api.getNodeForDomNodeId([]).length === 0, 'Empty ID array failed');
    const mixed = api.getDomNodeId([nodes[0], null, , nodes[1]]);
    check(
        mixed[0] === ids[0] && mixed[1] === null && mixed[2] === null &&
            mixed[3] === ids[1],
        'Mixed array failed');
    const mixedNodes = api.getNodeForDomNodeId([ids[0], -1, , ids[1]]);
    check(
        mixedNodes[0] === nodes[0] && mixedNodes[1] === null &&
            mixedNodes[2] === null && mixedNodes[3] === nodes[1],
        'Mixed IDs failed');
    // A large batch uses the same stable ID for repeated node references.
    const repeated = api.getDomNodeId(Array(512).fill(nodes[0]));
    check(repeated.every(id => id === ids[0]), 'Large batch failed');
    const marker = new Error('array getter');
    for (const method of ['getDomNodeId', 'getNodeForDomNodeId']) {
      const batch = [null];
      Object.defineProperty(batch, 0, {
        get() {
          throw marker;
        },
      });
      let error;
      try {
        api[method](batch);
      } catch (e) {
        error = e;
      }
      check(error === marker, 'Array getter exception was lost');
    }
    nodes[1].remove();
    check(api.getNodeForDomNodeId(ids[1]) === null, 'Detached ID accepted');
    check(api.getDomNodeId(nodes[1]) === null, 'Detached node accepted');
    // A retained node from another document must never escape the scope.
    const otherDocument = document.implementation.createHTMLDocument('other');
    check(
        api.getDomNodeId(otherDocument.body) === null,
        'Foreign document accepted');
    return true;
  }

  if (devtools) {
    chrome.test.runTests([
      function resolvesNodesWithDevToolsOpen() {
        // Reuse the identity checks in the extension's existing isolated world.
        // The ordinary page world must not gain access to this private API.
        chrome.devtools.inspectedWindow.eval(
            'typeof chrome.experimentalAiData', (result, exception) => {
              chrome.test.assertFalse(!!exception);
              chrome.test.assertEq('undefined', result);
              const script = `(${verifyNodes.toString()})(${
                  JSON.stringify(expectedError || null)})`;
              chrome.devtools.inspectedWindow.eval(
                  script, {useContentScriptContext: true},
                  (result, exception) => {
                    chrome.test.assertFalse(
                        !!exception, JSON.stringify(exception));
                    chrome.test.assertEq(true, result);
                    chrome.test.succeed();
                  });
            });
      },
    ]);
    return;
  }

  if (nodeAccess) {
    chrome.test.runTests([
      async function resolvesNodesInContentScript() {
        const results = await chrome.scripting.executeScript({
          target: {tabId},
          args: [expectedError || null, pseudoNodeId || null],
          func: verifyNodes,
        });
        chrome.test.assertEq(true, results[0].result);
        chrome.test.succeed();
      },
      async function keepsFrameDocumentsSeparate() {
        if (expectedError) {
          chrome.test.succeed();
          return;
        }
        const frames = await chrome.scripting.executeScript({
          target: {tabId, allFrames: true},
          func: () => chrome.experimentalAiData.getDomNodeId(document.body),
        });
        chrome.test.assertEq(3, frames.length);
        const ids = frames.map(frame => frame.result);
        chrome.test.assertEq(3, new Set(ids).size);
        const checks = await chrome.scripting.executeScript({
          target: {tabId, allFrames: true},
          args: [ids],
          func: ids => {
            const api = chrome.experimentalAiData;
            const ownId = api.getDomNodeId(document.body);
            // Both directions use the caller's document, even for child frames
            // that ordinary page script can access across the same origin.
            const child = document.querySelector('iframe');
            if (child &&
                api.getDomNodeId(child.contentDocument.body) !== null) {
              return false;
            }
            return api.getNodeForDomNodeId(ids).every(
                (node, i) =>
                    node === (ids[i] === ownId ? document.body : null));
          },
        });
        checks.forEach(check => chrome.test.assertEq(true, check.result));
        chrome.test.succeed();
      },
    ]);
    return;
  }

  if (expectedError) {
    chrome.test.runTests([
      async function rejectsUnauthorizedCapture() {
        for (const type of ['apc', 'screenshot', 'apc_and_screenshot']) {
          await chrome.test.assertPromiseRejects(
              capture({type}), `Error: ${expectedError}`);
        }
        chrome.test.succeed();
      },
    ]);
    return;
  }

  async function checkSnapshot(options, expectApc, expectScreenshot) {
    const snapshot = await capture(options);
    chrome.test.assertEq(expectApc, 'apcBase64' in snapshot);
    chrome.test.assertEq(expectScreenshot, 'screenshotBase64' in snapshot);
    if (expectApc) {
      // The page's text is embedded in the serialized APC protobuf.
      chrome.test.assertTrue(
          atob(snapshot.apcBase64).includes('APC snapshot browser test'));
    }
    if (expectScreenshot) {
      const bytes = atob(snapshot.screenshotBase64);
      chrome.test.assertEq('\x89PNG\r\n\x1a\n', bytes.slice(0, 8));
      const image = await createImageBitmap(new Blob(
          [Uint8Array.from(bytes, c => c.charCodeAt(0))], {type: 'image/png'}));
      chrome.test.assertTrue(image.width > 0);
      chrome.test.assertTrue(image.height > 0);
      image.close();
    }
  }

  chrome.test.runTests([
    async function defaultsToApc() {
      await checkSnapshot({}, true, false);
      chrome.test.succeed();
    },
    async function capturesApc() {
      for (const excludeActionableDetails of [false, true]) {
        await checkSnapshot(
            {type: 'apc', excludeActionableDetails}, true, false);
      }
      chrome.test.succeed();
    },
    async function capturesScreenshot() {
      for (const excludeActionableDetails of [false, true]) {
        await checkSnapshot(
            {type: 'screenshot', excludeActionableDetails}, false, true);
      }
      chrome.test.succeed();
    },
    async function capturesApcAndScreenshot() {
      for (const excludeActionableDetails of [false, true]) {
        await checkSnapshot(
            {type: 'apc_and_screenshot', excludeActionableDetails}, true, true);
      }
      chrome.test.succeed();
    },
    async function rejectsNegativeMetadataLimit() {
      await chrome.test.assertPromiseRejects(
          capture({maxMetaElements: -1}),
          'Error: maxMetaElements must not be negative.');
      chrome.test.succeed();
    },
    async function rejectsUnknownTab() {
      await chrome.test.assertPromiseRejects(
          chrome.experimentalAiData.getApcSnapshot(999999, {}),
          'Error: Invalid target tab passed in.');
      chrome.test.succeed();
    },
    async function rejectsUnknownSnapshotType() {
      chrome.test.assertThrows(
          () => capture({type: 'unknown'}),
          /Value must be one of apc, apc_and_screenshot, screenshot/);
      chrome.test.succeed();
    },
  ]);
});
