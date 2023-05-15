// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep in sync with ../extension_mojo/test.js when changing the test cases.

const iconUrl = 'https://foo.bar/icon.png';

const testCases = [
  async function AddApp() {
    chrome.enterprise.remoteApps.addApp({name: 'App 1', iconUrl}, () => {
      chrome.test.assertNoLastError();

      chrome.enterprise.remoteApps.addApp(
          {name: 'App 2', folderId: 'missing', iconUrl}, () => {
            chrome.test.assertLastError('Folder ID provided does not exist');

            chrome.test.succeed();
          });
    });
  },
  async function AddAppBadIconUrl() {
    chrome.enterprise.remoteApps.addApp({name: 'App 1', iconUrl: ''}, () => {
      chrome.test.assertNoLastError();

      chrome.test.succeed();
    });
  },
  async function AddAppNoIconUrl() {
    chrome.enterprise.remoteApps.addApp({name: 'App 1'}, () => {
      chrome.test.assertNoLastError();

      chrome.test.succeed();
    });
  },
  async function AddAppToFront() {
    chrome.enterprise.remoteApps.addApp(
        {name: 'App 1', iconUrl, addToFront: false}, (appId1) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Id 1', appId1);

          chrome.enterprise.remoteApps.addApp(
              {name: 'App 2', iconUrl, addToFront: true}, (appId2) => {
                chrome.test.assertNoLastError();
                chrome.test.assertEq('Id 2', appId2);

                chrome.test.succeed();
              });
        });
  },
  async function AddFolderAndApps() {
    chrome.enterprise.remoteApps.addFolder({name: 'Folder 1'}, (folderId) => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('Id 1', folderId);

      chrome.enterprise.remoteApps.addApp(
          {name: 'App 1', folderId, iconUrl}, (appId) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq('Id 2', appId);

            chrome.enterprise.remoteApps.addApp(
                {name: 'App 2', folderId, iconUrl}, (appId) => {
                  chrome.test.assertNoLastError();
                  chrome.test.assertEq('Id 3', appId);

                  chrome.test.succeed();
                });
          });
    });
  },
  async function AddFolderToFront() {
    chrome.enterprise.remoteApps.addApp({name: 'App 1', iconUrl}, (appId) => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('Id 1', appId);

      chrome.enterprise.remoteApps.addFolder(
          {name: 'Folder 1', addToFront: true}, (folderId) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq('Id 2', folderId);

            // App is not added to front since it has a parent folder.
            chrome.enterprise.remoteApps.addApp(
                {name: 'App 2', folderId, iconUrl}, (appId) => {
                  chrome.test.assertNoLastError();
                  chrome.test.assertEq('Id 3', appId);

                  chrome.test.succeed();
                });
          });
    });
  },
  async function OnRemoteAppLaunched() {
    chrome.enterprise.remoteApps.onRemoteAppLaunched.addListener((id) => {
      chrome.test.assertEq('Id 1', id);
      chrome.test.succeed();
    });

    chrome.enterprise.remoteApps.addApp({name: 'App 1', iconUrl}, () => {
      chrome.test.assertNoLastError();

      chrome.test.sendMessage('Remote app added');
    });
  },
  async function DeleteApp() {
    chrome.enterprise.remoteApps.addApp({name: 'App 1', iconUrl}, (appId) => {
      chrome.test.assertNoLastError();

      chrome.enterprise.remoteApps.deleteApp(appId, () => {
        chrome.test.assertNoLastError();

        chrome.enterprise.remoteApps.deleteApp(appId, () => {
          chrome.test.assertLastError('App ID provided does not exist');

          chrome.test.succeed();
        });
      });
    });
  },
  async function DeleteAppInFolder() {
    chrome.enterprise.remoteApps.addFolder({name: 'Folder 1'}, (folderId) => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('Id 1', folderId);

      chrome.enterprise.remoteApps.addApp(
          {name: 'App 1', folderId, iconUrl}, (appId) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq('Id 2', appId);

            chrome.enterprise.remoteApps.deleteApp(appId, () => {
              chrome.test.assertNoLastError();

              chrome.test.succeed();
            });
          });
    });
  },
  // Adds the following Remote apps and folders: `test app 5`, `Test App 7`,
  // `Test App 6 Folder`, `Test App 8` (contained by the folder) and sorts the
  // items after prompted by the calling site.
  async function AddRemoteItemsForSort() {
    chrome.enterprise.remoteApps.addApp(
        {name: 'test app 5', iconUrl, addToFront: true}, (appId) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Id 1', appId);

          chrome.enterprise.remoteApps.addApp(
              {name: 'Test App 7', iconUrl, addToFront: true}, (appId) => {
                chrome.test.assertNoLastError();
                chrome.test.assertEq('Id 2', appId);

                chrome.enterprise.remoteApps.addFolder(
                    {name: 'Test App 6 Folder', addToFront: true},
                    (folderId) => {
                      chrome.test.assertNoLastError();
                      chrome.test.assertEq('Id 3', folderId);

                      chrome.enterprise.remoteApps.addApp(
                          {name: 'Test App 8', folderId, iconUrl}, (appId) => {
                            chrome.test.assertNoLastError();
                            chrome.test.assertEq('Id 4', appId);

                            chrome.test.sendMessage('Ready to sort', () => {
                              // Sorts all launcher items with REMOTE_APPS_FIRST
                              // order.
                              chrome.enterprise.remoteApps.sortLauncher(
                                  {position: 'REMOTE_APPS_FIRST'}, () => {
                                    chrome.test.assertNoLastError();
                                    chrome.test.succeed();
                                  });
                            });
                          });
                    });
              });
        });
  },
  // Adds `test app 5` to the launcher and pins it to the shelf.
  async function PinSingleApp() {
    chrome.enterprise.remoteApps.addApp(
        {name: 'test app 5', iconUrl, addToFront: true}, (appId) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Id 1', appId);

          chrome.enterprise.remoteApps.setPinnedApps(['Id 1'], () => {
            chrome.test.assertNoLastError();
            chrome.test.succeed();
          });
        });
  },

  // Adds two apps to the launcher and tries to pin both of them to the shelf
  // which should result in an error.
  async function PinMultipleAppsError() {
    chrome.enterprise.remoteApps.addApp(
        {name: 'test app 5', iconUrl, addToFront: true}, (appId) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq('Id 1', appId);

          chrome.enterprise.remoteApps.addApp(
              {name: 'Test App 7', iconUrl, addToFront: true}, (appId) => {
                chrome.test.assertNoLastError();
                chrome.test.assertEq('Id 2', appId);

                chrome.enterprise.remoteApps.setPinnedApps(
                    ['Id 1', 'Id 2'], () => {
                      chrome.test.assertLastError(
                          'Pinning multiple apps is not yet supported');
                      chrome.test.succeed();
                    });
              });
        });
  },
];

chrome.test.getConfig(async (config) => {
  const testName = config.customArg;
  const testCase = testCases.find((f) => f.name === testName);
  if (!testCase) {
    chrome.test.notifyFail('Test case \'' + testName + '\' not found');
    return;
  }

  chrome.test.runTests([testCase]);
});
