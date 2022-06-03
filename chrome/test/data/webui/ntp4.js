// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/test/data/webui/ntp4_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * TestFixture for NTP4 WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function NTP4WebUITest() {}

NTP4WebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://newtab',
};

// Test loading new tab page and selecting each card doesn't have console
// errors.
// TODO(samarth): delete these tests along with the NTP4 code.
TEST_F('NTP4WebUITest', 'DISABLED_TestBrowsePages', function() {
  // This tests the ntp4 new tab page which is not used on touch builds.
  var cardSlider = ntp.getCardSlider();
  assertNotEquals(null, cardSlider);
  for (var i = 0; i < cardSlider.cardCount; i++) {
    cardSlider.selectCard(i);
    expectEquals(i, cardSlider.currentCard);
  }
});

// http://crbug.com/118944
TEST_F('NTP4WebUITest', 'DISABLED_NTPHasThumbnails', function() {
  var mostVisited = document.querySelectorAll('.most-visited');
  assertEquals(8, mostVisited.length, 'There should be 8 most visited tiles.');

  var apps = document.querySelectorAll('.app');
  if (loadTimeData.getBoolean('showApps')) {
    assertGE(apps.length, 1, 'There should be at least one app.');
  } else {
    assertEquals(0, apps.length, 'There should be no apps.');
  }
});

TEST_F('NTP4WebUITest', 'DISABLED_NTPHasNavDots', function() {
  var navDots = document.querySelectorAll('.dot');
  if (loadTimeData.getBoolean('showApps')) {
    assertGE(navDots.length, 2, 'There should be at least two navdots.');
  } else {
    assertEquals(1, navDots.length, 'There should be exactly one navdot.');
  }
});

// http://crbug.com/118514
TEST_F('NTP4WebUITest', 'DISABLED_NTPHasSelectedPageAndDot', function() {
  var selectedDot = document.querySelectorAll('.dot.selected');
  assertEquals(
      1, selectedDot.length, 'There should be exactly one selected dot.');

  var selectedTilePage = document.querySelectorAll('.tile-page.selected-card');
  assertEquals(
      1, selectedTilePage.length,
      'There should be exactly one selected tile page.');
});

TEST_F('NTP4WebUITest', 'DISABLED_NTPHasNoLoginNameWhenSignedOut', function() {
  var userName = document.querySelector('#login-status-header .profile-name');
  assertEquals(null, userName, 'Login name shouldn\'t exist when signed out.');
});

/**
 * Test fixture for NTP4 WebUI testing with login.
 * @extends {NTP4WebUITest}
 * @constructor
 */
function NTP4LoggedInWebUITest() {}

NTP4LoggedInWebUITest.prototype = {
  __proto__: NTP4WebUITest.prototype,

  /** @override */
  typedefCppFixture: 'NTP4LoggedInWebUITest',

  /** @override */
  testGenPreamble: function() {
    GEN('  SetLoginName("user@gmail.com");');
  },
};

// The following test is irrelevant to Chrome on Chrome OS.
GEN('#if !defined(OS_CHROMEOS)');

TEST_F(
    'NTP4LoggedInWebUITest', 'DISABLED_NTPHasLoginNameWhenSignedIn',
    function() {
      var userName =
          document.querySelector('#login-status-header .profile-name');
      assertNotEquals(
          userName, null, 'The logged-in user name can\'t be found.');
      assertEquals(
          'user@gmail.com', userName.textContent,
          'The user name should be present on the new tab.');
    });

GEN('#endif');
