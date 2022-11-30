// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function() {
    // Test management (backed by a json file) api enums.

    // The enum should be declared on the API object.
    chrome.test.assertTrue(
        'LaunchType' in chrome.management,
        '"LaunchType" is not present on chrome.management.');
    // The object should have entries for each enum entry. Note that we don't
    // test all entries here because we don't want to update this test if the
    // management api changes.
    chrome.test.assertTrue(
        'OPEN_AS_REGULAR_TAB' in chrome.management.LaunchType,
        '"OPEN_AS_REGULAR_TAB" is not present on management.LaunchType');
    // The value of the enum should be its string value.
    chrome.test.assertEq(chrome.management.LaunchType.OPEN_AS_REGULAR_TAB,
                         'OPEN_AS_REGULAR_TAB');
    // There should be more than one value for the enum.
    chrome.test.assertTrue(
        Object.keys(chrome.management.LaunchType).length > 1);

    // Perform an analogous test for the notifications api (backed by an idl).
    chrome.test.assertTrue(
        'PermissionLevel' in chrome.notifications,
        '"PermissionLevel" is not present on chrome.notifications.');
    chrome.test.assertTrue(
        'GRANTED' in chrome.notifications.PermissionLevel,
        '"GRANTED" is not present on notifications.PermissionLevel');
    chrome.test.assertEq(chrome.notifications.PermissionLevel.GRANTED,
                         'granted');
    chrome.test.assertTrue(
        Object.keys(chrome.notifications.PermissionLevel).length > 1);

    chrome.test.assertTrue('PlatformArch' in chrome.runtime,
                           '"PlatformArch" is not present on chrome.runtime.');
    chrome.test.assertTrue('X86_64' in chrome.runtime.PlatformArch,
                           '"X86_64" is not present on runtime.PlatformArch.');
    chrome.test.assertEq('x86-64', chrome.runtime.PlatformArch.X86_64);

    chrome.test.assertTrue(
        'OnInputEnteredDisposition' in chrome.omnibox,
        '"OnInputEnteredDisposition" is not present on chrome.runtime.');
    chrome.test.assertTrue(
        'NEW_FOREGROUND_TAB' in chrome.omnibox.OnInputEnteredDisposition,
        '"NEW_FOREGROUND_TAB" is not present on OnInputEnteredDisposition.');
    chrome.test.assertEq(
        'newForegroundTab',
        chrome.omnibox.OnInputEnteredDisposition.NEW_FOREGROUND_TAB);

    chrome.test.succeed();
  }
]);
