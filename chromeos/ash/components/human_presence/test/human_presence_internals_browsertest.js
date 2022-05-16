// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://hps-internals/
 */

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chromeos/dbus/human_presence/fake_human_presence_dbus_client.h"');
GEN('#include "chromeos/dbus/human_presence/human_presence_dbus_client.h"');

const HOST_ORIGIN = 'chrome://hps-internals';

// TODO:(crbug.com/1262025): We should avoid using `var`.
//
// js2gtest fixtures require var here (https://crbug.com/1033337).
// eslint-disable-next-line no-var
var HumanPresenceInternalsUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  testGenPreamble() {
    GEN(`
        chromeos::HumanPresenceDBusClient::InitializeFake();
        chromeos::FakeHumanPresenceDBusClient::Get()->Reset();
        chromeos::FakeHumanPresenceDBusClient::Get()->
            set_hps_service_is_available(true);
        chromeos::FakeHumanPresenceDBusClient::Get()->set_hps_sense_result(
            hps::HpsResult::POSITIVE);
        chromeos::FakeHumanPresenceDBusClient::Get()->set_hps_notify_result(
            hps::HpsResult::NEGATIVE);`);
  }
};

// Tests that chrome://hps-internals loads successfully.
TEST_F(
    'HumanPresenceInternalsUIBrowserTest', 'HasChromeSchemeURL', async () => {
      assertEquals(document.location.origin, HOST_ORIGIN);
      testDone();
    });

// Tests that the UI reflects the state of HPS.
TEST_F('HumanPresenceInternalsUIBrowserTest', 'StateSynchronized', async () => {
  document.querySelector('#root').addEventListener(
      'state-updated-for-test', () => {
        const senseState = document.querySelector('#sense-state').textContent;
        const notifyState = document.querySelector('#notify-state').textContent;
        // Wait until both features become enabled.
        if (senseState === 'disabled' || notifyState === 'disabled')
          return;
        assertEquals(senseState, 'positive');
        assertEquals(notifyState, 'negative');
        assertTrue(document.querySelector('#enable-sense').disabled);
        assertTrue(document.querySelector('#enable-notify').disabled);
        assertFalse(document.querySelector('#disable-sense').disabled);
        assertFalse(document.querySelector('#disable-notify').disabled);
        testDone();
      });
  // Enable a feature to trigger the UI state to update.
  document.querySelector('#enable-notify').click();
});
