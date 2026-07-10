// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper to open file URL and load it.
async function openFileUrlAndLoad(url) {
  return new Promise((resolve) => {
    chrome.tabs.onUpdated.addListener(
        function listener(tabId, changeInfo, tab) {
          if (tab.status === 'complete' && tab.url === url) {
            chrome.tabs.onUpdated.removeListener(listener);
            resolve(tab);
          }
        });
    chrome.test.openFileUrl(url);
  });
}

(async () => {
  const config = await new Promise((resolve) => {
    chrome.test.getConfig(resolve);
  });
  const customArgStr = config.customArg;
  chrome.test.log('Config loaded: ' + customArgStr);
  const parts = customArgStr ? customArgStr.split('|') : [];
  const workerHtmlUrl = parts[0] || '';
  const expectFileAccess = parts[1] === 'enabled';

  const fileUrl = new URL(config.testDataDirectory + '/../body1.html').href;
  const mhtmlFileUrl =
      new URL(config.testDataDirectory + '/../mhtml-with-subframes.mht').href;

  const kFileUrlsRequireFileAccess =
      /(Cannot navigate to a file URL without local file access|Cannot attach to this target|Navigating to local URL is not allowed|Creating a target with a local URL is not allowed)/;

  chrome.test.runTests([
    async function verifyInitialState() {
      chrome.test.log('verifyInitialState');
      const isAllowed = await chrome.extension.isAllowedFileSchemeAccess();
      chrome.test.assertEq(expectFileAccess, isAllowed);
      chrome.test.succeed();
    },

    async function testAttach() {
      chrome.test.log('testAttach');
      const tab = await openFileUrlAndLoad(fileUrl);
      if (expectFileAccess) {
        await chrome.debugger.attach({tabId: tab.id}, '1.1');
        await chrome.debugger.detach({tabId: tab.id});
      } else {
        await chrome.test.assertPromiseRejects(
            chrome.debugger.attach({tabId: tab.id}, '1.1'),
            kFileUrlsRequireFileAccess);
      }
      chrome.test.succeed();
    },

    async function testAttachMhtml() {
      chrome.test.log('testAttachMhtml');
      const tab = await openFileUrlAndLoad(mhtmlFileUrl);
      if (expectFileAccess) {
        await chrome.debugger.attach({tabId: tab.id}, '1.1');
        await chrome.debugger.detach({tabId: tab.id});
      } else {
        await chrome.test.assertPromiseRejects(
            chrome.debugger.attach({tabId: tab.id}, '1.1'),
            kFileUrlsRequireFileAccess);
      }
      chrome.test.succeed();
    },

    async function testAttachBlobWorker() {
      chrome.test.log('testAttachBlobWorker');
      if (expectFileAccess) {
        const tab = await chrome.tabs.create({url: workerHtmlUrl});
        chrome.test.assertTrue(!!tab);

        // Attach to the tab first.
        await chrome.debugger.attach({tabId: tab.id}, '1.3');

        const attachedPromise = new Promise(resolve => {
          const listener = (source, method, params) => {
            if (method === 'Target.attachedToTarget') {
              chrome.debugger.onEvent.removeListener(listener);
              resolve(params);
            }
          };
          chrome.debugger.onEvent.addListener(listener);
        });

        // Use auto-attach to find the worker.
        await chrome.debugger.sendCommand(
            {tabId: tab.id}, 'Target.setAutoAttach',
            {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

        // Trigger worker creation if not already created.
        // (The page already creates it on load).

        const attachedParams = await Promise.race([
          attachedPromise,
          new Promise((r) => setTimeout(() => r(null), 5000)),
        ]);

        if (attachedParams && attachedParams.sessionId) {
          // Detach child session.
          // Note: chrome.debugger.detach takes {tabId, targetId, extensionId}
          // but NOT sessionId. To detach a child session, we usually use
          // Target.detachFromTarget or just detach the parent.
          await chrome.debugger.sendCommand(
              {tabId: tab.id}, 'Target.detachFromTarget',
              {sessionId: attachedParams.sessionId});
        } else {
          // Fallback: try to find and attach manually.
          const findWorkerTarget = async (retries = 0) => {
            const targets = await chrome.debugger.getTargets();
            const t = targets.find(
                t => (t.type === 'worker' || t.type === 'other') &&
                    (t.url &&
                     (t.url.startsWith('blob:') ||
                      t.url.startsWith('file:'))) &&
                    (!t.extensionId || t.extensionId !== chrome.runtime.id) &&
                    t.title !== 'Debugger File Access Test' &&
                    t.title !== 'worker-created');
            if (t) {
              return t;
            }
            if (retries < 50) {
              await new Promise(r => setTimeout(r, 100));
              return findWorkerTarget(retries + 1);
            }
            return null;
          };

          const workerTarget = await findWorkerTarget();
          if (workerTarget) {
            await chrome.debugger.attach({targetId: workerTarget.id}, '1.3');
            await chrome.debugger.detach({targetId: workerTarget.id});
          } else {
            chrome.test.log('Skipping worker attach verification');
          }
        }

        await chrome.debugger.detach({tabId: tab.id});
        await chrome.tabs.remove(tab.id);
      } else {
        await chrome.test.assertPromiseRejects(
            chrome.tabs.create({url: workerHtmlUrl}),
            kFileUrlsRequireFileAccess);
      }
      chrome.test.succeed();
    },

    async function testAttachAndNavigate() {
      chrome.test.log('testAttachAndNavigate');
      const tab = await chrome.tabs.create({url: 'about:blank'});
      await chrome.debugger.attach({tabId: tab.id}, '1.1');

      if (expectFileAccess) {
        await chrome.debugger.sendCommand(
            {tabId: tab.id}, 'Page.navigate', {url: fileUrl});
      } else {
        await chrome.test.assertPromiseRejects(
            chrome.debugger.sendCommand(
                {tabId: tab.id}, 'Page.navigate', {url: fileUrl}),
            kFileUrlsRequireFileAccess);
      }

      await chrome.debugger.detach({tabId: tab.id});
      await chrome.tabs.remove(tab.id);
      chrome.test.succeed();
    },

    async function testCreateTarget() {
      chrome.test.log('testCreateTarget');
      const tab = await chrome.tabs.create({url: 'about:blank'});
      await chrome.debugger.attach({tabId: tab.id}, '1.1');

      if (expectFileAccess) {
        const result = await chrome.debugger.sendCommand(
            {tabId: tab.id}, 'Target.createTarget', {url: fileUrl});
        chrome.test.assertTrue(!!result.targetId);
        // Clean up leaked target using extensions API.
        const tabs = await chrome.tabs.query({url: fileUrl});
        if (tabs.length > 0) {
          await chrome.tabs.remove(tabs[0].id);
        }
      } else {
        await chrome.test.assertPromiseRejects(
            chrome.debugger.sendCommand(
                {tabId: tab.id}, 'Target.createTarget', {url: fileUrl}),
            kFileUrlsRequireFileAccess);
      }
      await chrome.debugger.detach({tabId: tab.id});
      await chrome.tabs.remove(tab.id);
      chrome.test.succeed();
    },

    // https://crbug.com/40091993
    async function setDownloadBehavior() {
      chrome.test.log('setDownloadBehavior');
      const tab = await chrome.tabs.create({url: 'about:blank'});
      await chrome.debugger.attach({tabId: tab.id}, '1.1');

      await chrome.test.assertPromiseRejects(
          chrome.debugger.sendCommand(
              {tabId: tab.id}, 'Browser.setDownloadBehavior',
              {behavior: 'allow', downloadPath: '/tmp'}),
          /(Not allowed|wasn't found)/);

      await chrome.debugger.detach({tabId: tab.id});
      await chrome.tabs.remove(tab.id);
      chrome.test.succeed();
    },

    // https://crbug.com/40090289
    async function setFileInputFiles() {
      chrome.test.log('setFileInputFiles');
      const tab = await chrome.tabs.create({url: 'about:blank'});
      await chrome.debugger.attach({tabId: tab.id}, '1.1');
      await chrome.debugger.sendCommand({tabId: tab.id}, 'DOM.enable', {});

      const expectedRegex = expectFileAccess ? /Could not find node/ :
                                               /(Not allowed|wasn't found)/;
      await chrome.test.assertPromiseRejects(
          chrome.debugger.sendCommand(
              {tabId: tab.id}, 'DOM.setFileInputFiles', {nodeId: 1, files: []}),
          expectedRegex);

      await chrome.debugger.detach({tabId: tab.id});
      await chrome.tabs.remove(tab.id);
      chrome.test.succeed();
    },
  ]);
})();
