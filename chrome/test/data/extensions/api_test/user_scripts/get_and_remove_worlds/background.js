// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const configForDefaultWorld =
    {
      csp: 'test csp',
      messaging: true,
    };

const configForOtherWorld =
    {
      csp: 'another csp',
      messaging: false,
      worldId: 'some other id',
    };

function sortWorlds(worlds) {
  function worldIdOrDefault(id) {
    return id || '<default>';
  }

  worlds.sort((a, b) => {
    return worldIdOrDefault(a.id) < worldIdOrDefault(b.worldId);
  });

  return worlds;
}

chrome.test.runTests([
  async function noWorldsReturnedWhenNoneConfigured() {
    const worlds = await chrome.userScripts.getWorldConfigurations();
    chrome.test.assertEq([], worlds);
    chrome.test.succeed();
  },

  async function retrieveDefaultWorldConfiguration() {
    await chrome.userScripts.configureWorld(configForDefaultWorld);
    const worlds = await chrome.userScripts.getWorldConfigurations();
    chrome.test.assertEq([configForDefaultWorld], worlds);

    // TODO(https://crbug.com/331680187): Remove world config in order to clean
    // up state between tests once the API to remove configs is added.
    chrome.test.succeed();
  },

  async function retrieveAdditionalWorldConfig() {
    // Note: Right now, the default world config is still added.

    // Add a second world config.
    await chrome.userScripts.configureWorld(configForOtherWorld);
    const worlds = await chrome.userScripts.getWorldConfigurations();

    chrome.test.assertEq(
        sortWorlds([configForDefaultWorld, configForOtherWorld]),
        sortWorlds(worlds));

    // TODO(https://crbug.com/331680187): Remove world config in order to clean
    // up state between tests once the API to remove configs is added.
    chrome.test.succeed();
  },
]);
