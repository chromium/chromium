// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const otherWorldId = 'some other id';

const configForDefaultWorld =
    {
      csp: 'test csp',
      messaging: true,
    };

const configForOtherWorld =
    {
      csp: 'another csp',
      messaging: false,
      worldId: otherWorldId,
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

  async function retrieveAndRemoveDefaultWorldConfiguration() {
    await chrome.userScripts.configureWorld(configForDefaultWorld);
    let worlds = await chrome.userScripts.getWorldConfigurations();
    chrome.test.assertEq([configForDefaultWorld], worlds);

    // Remove the default world configuration.
    await chrome.userScripts.resetWorldConfiguration();

    // There should no longer be any registered configurations.
    worlds = await chrome.userScripts.getWorldConfigurations();
    chrome.test.assertEq([], worlds);

    chrome.test.succeed();
  },

  async function retrieveAndRemoveAdditionalWorldConfig() {
    // Add a non-default world config.
    await chrome.userScripts.configureWorld(configForOtherWorld);
    let worlds = await chrome.userScripts.getWorldConfigurations();

    chrome.test.assertEq([configForOtherWorld], worlds);

    // Add the default world config back in.
    await chrome.userScripts.configureWorld(configForDefaultWorld);
    worlds = await chrome.userScripts.getWorldConfigurations();

    chrome.test.assertEq(
        sortWorlds([configForDefaultWorld, configForOtherWorld]),
        sortWorlds(worlds));

    // Remove the non-default config.
    await chrome.userScripts.resetWorldConfiguration(otherWorldId);
    worlds = await chrome.userScripts.getWorldConfigurations();

    // Only the default world config should remain.
    chrome.test.assertEq([configForDefaultWorld], worlds);

    // Clean up.
    await chrome.userScripts.resetWorldConfiguration();

    chrome.test.succeed();
  },

  async function callingResetWorldConfigurationForUnregisteredIdDoesNothing() {
    // Register a config for a non-default world.
    await chrome.userScripts.configureWorld(configForOtherWorld);
    // Try to reset another, unregistered world's config.
    await chrome.userScripts.resetWorldConfiguration('unregistered id');

    // The other world's config should be unchanged.
    const worlds = await chrome.userScripts.getWorldConfigurations();
    chrome.test.assertEq([configForOtherWorld], worlds);

    // Clean up.
    await chrome.userScripts.resetWorldConfiguration(otherWorldId);

    chrome.test.succeed();
  },

  async function callingResetWithInvalidIdsFails() {
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.resetWorldConfiguration(''),
        'Error: If specified, `worldId` must be non-empty.');

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.resetWorldConfiguration('_foo'),
        `Error: World IDs beginning with '_' are reserved.`);

    await chrome.test.assertPromiseRejects(
        chrome.userScripts.resetWorldConfiguration('a'.repeat(257)),
        'Error: World IDs must be at most 256 characters.');

    chrome.test.succeed();
  },
]);
