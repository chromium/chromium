// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import type {OnDeviceAiCapability, SettingsOnDeviceAiPageEntryElement} from 'chrome://settings/lazy_load.js';
import {OnDeviceModelCategory, OnDeviceModelStatus} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createAsrCapability, createGemmaCapability} from './on_device_ai_page_test_util.js';

suite('OnDeviceAiPageEntry', function() {
  let entry: SettingsOnDeviceAiPageEntryElement;

  function getDeleteButton(): HTMLElement|null {
    return entry.shadowRoot.querySelector<HTMLElement>('#delete');
  }

  function getStatusIcon(): CrIconElement|null {
    return entry.shadowRoot.querySelector<CrIconElement>('#statusIcon');
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entry = document.createElement('settings-on-device-ai-page-entry');
    entry.capability = createGemmaCapability();
    document.body.appendChild(entry);
    await microtasksFinished();
  });

  function assertCapabilityRendered(capability: OnDeviceAiCapability) {
    assertEquals(
        capability.categoryTitle, entry.$.modelTitle.textContent.trim());
    assertEquals(
        capability.description, entry.$.modelDescription.textContent.trim());
    assertEquals(
        loadTimeData.getStringF(
            'onDeviceAiModelMetadata', capability.modelName, capability.size),
        entry.$.modelMetadata.textContent.trim());
    assertEquals(
        loadTimeData.getStringF(
            'onDeviceAiModelExpandA11yLabel', capability.categoryTitle),
        entry.$.expand.ariaLabel);
    const deleteButton = getDeleteButton();
    assertTrue(!!deleteButton);
    assertNull(getStatusIcon());
    assertEquals(
        loadTimeData.getStringF(
            'onDeviceAiModelDeleteA11yLabel', capability.categoryTitle),
        deleteButton.ariaLabel);
  }

  async function assertInProgressStatusRendered(
      status: OnDeviceModelStatus.DOWNLOADING|OnDeviceModelStatus.UPDATING,
      expectedMetadataStringId: string, expectedIcon: string) {
    const capability = createGemmaCapability({status});
    entry.capability = capability;
    await microtasksFinished();

    assertEquals(
        capability.categoryTitle, entry.$.modelTitle.textContent.trim());
    assertEquals(
        capability.description, entry.$.modelDescription.textContent.trim());
    assertEquals(
        loadTimeData.getString(expectedMetadataStringId),
        entry.$.modelMetadata.textContent.trim());
    assertNull(getDeleteButton());
    const statusIcon = getStatusIcon();
    assertTrue(!!statusIcon);
    assertTrue(isVisible(statusIcon));
    assertEquals(expectedIcon, statusIcon.icon);
  }

  test('renders and updates capability details', async function() {
    // Verify initial render using the capability set in setup().
    assertCapabilityRendered(createGemmaCapability());

    // Verify subsequent updates when the capability property changes.
    entry.capability = createAsrCapability();
    await microtasksFinished();
    assertCapabilityRendered(createAsrCapability());
  });

  test('leading icon matches model category', async function() {
    const expectedIcons: Array<[OnDeviceModelCategory, string]> = [
      [OnDeviceModelCategory.GENERAL_PURPOSE, 'settings20:pen-spark'],
      [OnDeviceModelCategory.PROOFREADING, 'settings20:spellcheck-spark'],
      [OnDeviceModelCategory.SUMMARIZING, 'settings20:summarize-auto-2'],
      [OnDeviceModelCategory.LIVE_TRANSCRIPTION, 'settings20:mic-detect-auto'],
    ];
    for (const [category, icon] of expectedIcons) {
      entry.capability = {...createGemmaCapability(), category};
      await microtasksFinished();
      assertEquals(icon, entry.$.modelIcon.icon);
    }
  });

  test('renders downloading status', async function() {
    await assertInProgressStatusRendered(
        OnDeviceModelStatus.DOWNLOADING, 'onDeviceAiModelDownloading',
        'settings20:downloading');
  });

  test('renders updating status', async function() {
    await assertInProgressStatusRendered(
        OnDeviceModelStatus.UPDATING, 'onDeviceAiModelUpdating',
        'settings20:update');
  });

  test('expand button toggles details', async function() {
    assertFalse(entry.$.expand.expanded);
    assertFalse(entry.$.collapse.opened);

    entry.$.expand.click();
    await microtasksFinished();
    assertTrue(entry.$.expand.expanded);
    assertTrue(entry.$.collapse.opened);

    const details = createGemmaCapability().details;
    const bullets = entry.$.modelDetailsList.querySelectorAll('li');
    assertEquals(details.length, bullets.length);
    bullets.forEach((bullet, i) => {
      assertTrue(isVisible(bullet));
      assertEquals(
          details[i], bullet.querySelector('span')!.textContent.trim());
    });

    entry.$.expand.click();
    await microtasksFinished();
    assertFalse(entry.$.expand.expanded);
    assertFalse(entry.$.collapse.opened);
  });

  test('delete button fires delete-click event', async function() {
    const deleteEventPromise =
        eventToPromise<CustomEvent<number>>('delete-click', entry);
    const deleteButton = getDeleteButton();
    assertTrue(!!deleteButton);
    deleteButton.click();
    const event = await deleteEventPromise;
    assertEquals(createGemmaCapability().id, event.detail);
  });
});
