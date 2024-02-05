// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsChannelSwitcherDialogElement} from 'chrome://os-settings/lazy_load.js';
import {AboutPageBrowserProxyImpl, BrowserChannel} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestAboutPageBrowserProxy} from './test_about_page_browser_proxy.js';

suite('<settings-channel-switcher-dialog>', () => {
  let dialog: SettingsChannelSwitcherDialogElement;
  let browserProxy: TestAboutPageBrowserProxy;
  let currentChannel: BrowserChannel;

  setup(async () => {
    currentChannel = BrowserChannel.BETA;
    browserProxy = new TestAboutPageBrowserProxy();

    browserProxy.setChannels(currentChannel, currentChannel);
    AboutPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    clearBody();
    dialog = document.createElement('settings-channel-switcher-dialog');
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('getChannelInfo');
  });

  test('Dialog is initialized properly', () => {
    const radioGroup = dialog.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);

    const {changeChannel, changeChannelAndPowerwash, warningSelector} =
        dialog.$;

    // Check that upon initialization the radio button corresponding to
    // the current release channel is pre-selected.
    assertEquals(currentChannel, radioGroup.selected);
    assertEquals(-1, warningSelector.selected);

    // Check that action buttons are hidden when current and target
    // channel are the same.
    assertFalse(isVisible(changeChannel));
    assertFalse(isVisible(changeChannelAndPowerwash));
  });

  // Test case where user switches to a less stable channel.
  test('Change to a less stable channel', async () => {
    const radioButtons = dialog.shadowRoot!.querySelectorAll('cr-radio-button');
    assertEquals(3, radioButtons.length);

    assertEquals(BrowserChannel.DEV, radioButtons.item(2).name);
    radioButtons.item(2).click();
    flush();
    await browserProxy.whenCalled('getChannelInfo');

    const {changeChannel, changeChannelAndPowerwash, warningSelector} =
        dialog.$;
    assertEquals(2, warningSelector.selected);

    // Check that only the "Change channel" button becomes visible.
    assertTrue(isVisible(changeChannel));
    assertFalse(isVisible(changeChannelAndPowerwash));

    const whenTargetChannelChangedFired =
        eventToPromise('target-channel-changed', dialog);

    changeChannel.click();
    const [channel, isPowerwashAllowed] =
        await browserProxy.whenCalled('setChannel');
    assertEquals(BrowserChannel.DEV, channel);
    assertFalse(isPowerwashAllowed);
    const {detail} = await whenTargetChannelChangedFired;
    assertEquals(BrowserChannel.DEV, detail);
  });

  // Test case where user switches to a more stable channel.
  test('Change to a more stable channel', async () => {
    const radioButtons = dialog.shadowRoot!.querySelectorAll('cr-radio-button');
    assertEquals(3, radioButtons.length);

    assertEquals(BrowserChannel.STABLE, radioButtons.item(0).name);
    radioButtons.item(0).click();
    flush();
    await browserProxy.whenCalled('getChannelInfo');

    const {changeChannel, changeChannelAndPowerwash, warningSelector} =
        dialog.$;
    assertEquals(1, warningSelector.selected);

    // Check that only the "Change channel and Powerwash" button becomes
    // visible.
    assertFalse(isVisible(changeChannel));
    assertTrue(isVisible(changeChannelAndPowerwash));

    const whenTargetChannelChangedFired =
        eventToPromise('target-channel-changed', dialog);

    changeChannelAndPowerwash.click();
    const [channel, isPowerwashAllowed] =
        await browserProxy.whenCalled('setChannel');
    assertEquals(BrowserChannel.STABLE, channel);
    assertTrue(isPowerwashAllowed);
    const {detail} = await whenTargetChannelChangedFired;
    assertEquals(BrowserChannel.STABLE, detail);
  });
});
