// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bluetooth-pairing/strings.m.js';
import 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_enter_code_page.js';

import type {SettingsBluetoothPairingEnterCodeElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_pairing_enter_code_page.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../chai_assert.js';

suite('CrComponentsBluetoothPairingEnterCodePageTest', function() {
  let bluetoothPairingEnterCodePage: SettingsBluetoothPairingEnterCodeElement;

  async function flushAsync(): Promise<null> {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    bluetoothPairingEnterCodePage =
        document.createElement('bluetooth-pairing-enter-code-page');
    document.body.appendChild(bluetoothPairingEnterCodePage);
    assertTrue(!!bluetoothPairingEnterCodePage);
    flush();
  });

  test('UI states', async function() {
    const getKeys = () =>
        bluetoothPairingEnterCodePage.shadowRoot!.querySelectorAll('.key');
    const getEnter = () =>
        bluetoothPairingEnterCodePage.shadowRoot!.querySelector('#enter');

    const deviceName = 'BeatsX';
    bluetoothPairingEnterCodePage.deviceName = deviceName;
    await flushAsync();

    const message =
        bluetoothPairingEnterCodePage.shadowRoot!.querySelector('#message');

    assertEquals(
        bluetoothPairingEnterCodePage.i18n(
            'bluetoothPairingEnterKeys', deviceName),
        message!.textContent!.trim());

    const defaultKeyClass = 'center key ';
    const nextKeyClass = defaultKeyClass + 'next';
    const typedKeyClass = defaultKeyClass + 'typed';
    const defaultEnterClass = 'center enter ';
    const nextEnterClass = defaultEnterClass + 'next';

    bluetoothPairingEnterCodePage.code = '123456';
    bluetoothPairingEnterCodePage.numKeysEntered = 0;
    await flushAsync();

    let keys = getKeys();
    assertTrue(!!keys.length);
    assertEquals(keys.length, 6);
    assertEquals(keys[0]!.className, defaultKeyClass);
    assertEquals(keys[1]!.className, defaultKeyClass);
    assertEquals(keys[5]!.className, defaultKeyClass);
    assertEquals(getEnter()!.className, defaultEnterClass);

    bluetoothPairingEnterCodePage.numKeysEntered = 2;
    await flushAsync();

    keys = getKeys();
    assertTrue(!!keys.length);
    assertTrue(keys.length >= 6);
    assertEquals(keys[2]!.className, nextKeyClass);
    assertEquals(keys[1]!.className, typedKeyClass);
    assertEquals(keys[5]!.className, defaultKeyClass);
    assertEquals(getEnter()!.className, defaultEnterClass);

    bluetoothPairingEnterCodePage.numKeysEntered = 6;
    await flushAsync();

    keys = getKeys();
    assertTrue(!!keys.length);
    assertTrue(keys.length >= 6);
    assertEquals(keys[1]!.className, typedKeyClass);
    assertEquals(keys[5]!.className, typedKeyClass);
    assertEquals(getEnter()!.className, nextEnterClass);
  });

  test('Changing PinCode', async function() {
    const getKeys = () =>
        bluetoothPairingEnterCodePage.shadowRoot!.querySelectorAll('.key');

    const deviceName = 'BeatsX';
    bluetoothPairingEnterCodePage.deviceName = deviceName;
    bluetoothPairingEnterCodePage.code = '123456';
    bluetoothPairingEnterCodePage.numKeysEntered = 0;
    await flushAsync();

    let keys = getKeys();
    assertTrue(!!keys.length);
    assertTrue(keys.length >= 6);
    assertEquals(keys[0]!.textContent!.trim(), '1');
    assertEquals(keys[1]!.textContent!.trim(), '2');
    assertEquals(keys[5]!.textContent!.trim(), '6');

    bluetoothPairingEnterCodePage.code = '987654';
    await flushAsync();

    keys = getKeys();
    assertTrue(!!keys.length);
    assertTrue(keys.length >= 6);
    assertEquals(keys[0]!.textContent!.trim(), '9');
    assertEquals(keys[1]!.textContent!.trim(), '8');
    assertEquals(keys[5]!.textContent!.trim(), '4');
  });
});
