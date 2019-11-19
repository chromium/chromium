// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('controlled button', function() {
  /** @type {ControlledButtonElement} */
  let controlledButton;

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const uncontrolledPref = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true
  };

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const extensionControlledPref = Object.assign(
      {
        controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      },
      uncontrolledPref);

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const policyControlledPref = Object.assign(
      {
        controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      },
      uncontrolledPref);

  setup(function() {
    PolymerTest.clearBody();
    controlledButton = document.createElement('controlled-button');
    controlledButton.pref = uncontrolledPref;
    document.body.appendChild(controlledButton);
    Polymer.dom.flush();
  });

  test('controlled prefs', function() {
    assertFalse(controlledButton.$$('cr-button').disabled);
    assertFalse(!!controlledButton.$$('cr-policy-pref-indicator'));

    controlledButton.pref = extensionControlledPref;
    Polymer.dom.flush();
    assertTrue(controlledButton.$$('cr-button').disabled);
    assertTrue(!!controlledButton.$$('cr-policy-pref-indicator'));

    controlledButton.pref = policyControlledPref;
    Polymer.dom.flush();
    assertTrue(controlledButton.$$('cr-button').disabled);
    const indicator = controlledButton.$$('cr-policy-pref-indicator');
    assertTrue(!!indicator);
    assertGT(indicator.clientHeight, 0);

    controlledButton.pref = uncontrolledPref;
    Polymer.dom.flush();
    assertFalse(controlledButton.$$('cr-button').disabled);
    assertFalse(!!controlledButton.$$('cr-policy-pref-indicator'));
  });

  test('null pref', function() {
    controlledButton.pref = extensionControlledPref;
    Polymer.dom.flush();
    assertTrue(controlledButton.$$('cr-button').disabled);
    assertTrue(!!controlledButton.$$('cr-policy-pref-indicator'));

    controlledButton.pref = null;
    Polymer.dom.flush();
    assertFalse(controlledButton.$$('cr-button').disabled);
    assertFalse(!!controlledButton.$$('cr-policy-pref-indicator'));
  });

  test('action-button', function() {
    assertNotEquals(
        'action-button', controlledButton.$$('cr-button').className);

    const controlledActionButton = document.createElement('controlled-button');
    controlledActionButton.pref = uncontrolledPref;
    controlledActionButton.className = 'action-button';
    document.body.appendChild(controlledActionButton);
    Polymer.dom.flush();
    assertEquals(
        'action-button', controlledActionButton.$$('cr-button').className);
  });
});
