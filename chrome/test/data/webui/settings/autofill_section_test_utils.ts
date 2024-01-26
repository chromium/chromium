// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CountryDetailManager, SettingsAddressEditDialogElement, SettingsAddressRemoveConfirmationDialogElement, SettingsAutofillSectionElement} from 'chrome://settings/lazy_load.js';
import {AutofillManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createAddressEntry, TestAutofillManager} from './autofill_fake_data.js';
// clang-format on

/**
 * Test implementation.
 */
export class CountryDetailManagerTestImpl implements CountryDetailManager {
  getCountryList() {
    return new Promise<chrome.autofillPrivate.CountryEntry[]>(function(
        resolve) {
      resolve([
        {name: 'United States', countryCode: 'US'},  // Default test country.
        {name: 'Israel', countryCode: 'IL'},
        {name: 'United Kingdom', countryCode: 'GB'},
      ]);
    });
  }

  getAddressFormat(countryCode: string) {
    return chrome.autofillPrivate.getAddressComponents(countryCode);
  }
}


/**
 * Resolves the promise after the element fires the expected event. |causeEvent|
 * is called after adding a listener to make sure that the event is captured.
 */
export function expectEvent(
    element: Element, eventName: string, causeEvent: () => void) {
  const promise = eventToPromise(eventName, element);
  causeEvent();
  return promise;
}

/**
 * Creates the autofill section for the given list.
 */
export async function createAutofillSection(
    addresses: chrome.autofillPrivate.AddressEntry[], prefValues: any,
    accountInfo?: chrome.autofillPrivate.AccountInfo):
    Promise<SettingsAutofillSectionElement> {
  // Override the AutofillManagerImpl for testing.
  const autofillManager = new TestAutofillManager();
  autofillManager.data.addresses = addresses;
  if (accountInfo) {
    autofillManager.data.accountInfo = accountInfo;
  }
  AutofillManagerImpl.setInstance(autofillManager);

  const section = document.createElement('settings-autofill-section');
  section.prefs = {autofill: prefValues};
  document.body.appendChild(section);
  await autofillManager.whenCalled('getAddressList');

  return section;
}

/**
 * Creates the Edit Address dialog and fulfills the promise when the dialog
 * has actually opened.
 */
export function createAddressDialog(
    address: chrome.autofillPrivate.AddressEntry,
    accountInfo?: chrome.autofillPrivate.AccountInfo):
    Promise<SettingsAddressEditDialogElement> {
  return new Promise(function(resolve) {
    const section = document.createElement('settings-address-edit-dialog');
    section.address = address;
    section.accountInfo = accountInfo;
    document.body.appendChild(section);
    eventToPromise('on-update-address-wrapper', section).then(function() {
      resolve(section);
    });
  });
}

export async function openAddressDialog(
    section: SettingsAutofillSectionElement):
    Promise<SettingsAddressEditDialogElement> {
  let dialog =
      section.shadowRoot!.querySelector('settings-address-edit-dialog');
  assertFalse(!!dialog, 'stale dialog found');

  section.$.addAddress.click();

  flush();

  dialog = section.shadowRoot!.querySelector('settings-address-edit-dialog');

  assertTrue(!!dialog, 'the dialog element should be in the section subtree');

  await eventToPromise('on-update-address-wrapper', dialog);
  return dialog;
}

/**
 * Opens and returns the address edit dialog element for specified
 * by |index| address in the |section| list.
 */
export async function initiateEditing(
    section: SettingsAutofillSectionElement,
    index: number): Promise<SettingsAddressEditDialogElement> {
  let dialog =
      section.shadowRoot!.querySelector<SettingsAddressEditDialogElement>(
          'settings-address-edit-dialog');
  assertFalse(!!dialog, 'stale dialog found');

  const addressElements = section.$.addressList.children;

  assertGT(
      addressElements.length, index,
      'index is too high, not enough addresses in the list');

  const menu =
      addressElements[index]!.querySelector<HTMLElement>('.address-menu');

  assertTrue(!!menu, 'the row element should contain the menu element');

  // Open menu and click the Edit button.
  menu.click();
  section.$.menuEditAddress.click();

  flush();

  dialog = section.shadowRoot!.querySelector<SettingsAddressEditDialogElement>(
      'settings-address-edit-dialog');

  assertTrue(!!dialog, 'the dialog element should be in the section subtree');

  await eventToPromise('on-update-address-wrapper', dialog);
  return dialog;
}

/**
 * Opens and returns the remove confirmation dialog element for specified
 * by |index| address in the |section| list.
 */
export function initiateRemoving(
    section: SettingsAutofillSectionElement,
    index: number): SettingsAddressRemoveConfirmationDialogElement {
  let dialog =
      section.shadowRoot!
          .querySelector<SettingsAddressRemoveConfirmationDialogElement>(
              'settings-address-remove-confirmation-dialog');
  assertFalse(!!dialog, 'stale dialog found');

  const addressElements = section.$.addressList.children;

  assertGT(
      addressElements.length, index,
      'index is too high, not enough addresses in the list');

  const menu =
      addressElements[index]!.querySelector<HTMLElement>('.address-menu');

  assertTrue(!!menu, 'the row element should contain the menu element');

  // Open menu and click the Delete button.
  menu.click();
  section.$.menuRemoveAddress.click();

  flush();

  dialog = section.shadowRoot!
               .querySelector<SettingsAddressRemoveConfirmationDialogElement>(
                   'settings-address-remove-confirmation-dialog');

  assertTrue(!!dialog, 'the dialog element should be in the section subtree');

  return dialog;
}

/**
 * Creates the remove address dialog. Simulate clicking "Remove" button in
 * autofill section.
 */
export async function createRemoveAddressDialog(
    autofillManager: TestAutofillManager):
    Promise<SettingsAddressRemoveConfirmationDialogElement> {
  const address = createAddressEntry();

  // Override the AutofillManagerImpl for testing.
  autofillManager.data.addresses = [address];
  AutofillManagerImpl.setInstance(autofillManager);

  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const section = document.createElement('settings-autofill-section');
  document.body.appendChild(section);
  await flushTasks();

  return initiateRemoving(section, 0);
}

/**
 * Performs some UI and manager manipulations to simulate the address removal.
 */
export async function deleteAddress(
    section: SettingsAutofillSectionElement, manager: TestAutofillManager,
    index: number) {
  const dialog = await initiateRemoving(section, index);
  const closePromise = eventToPromise('close', dialog.$.dialog);
  dialog.$.remove.click();
  await closePromise;

  const address = [...manager.data.addresses];
  address.splice(index, 1);
  manager.data.addresses = address;
  manager.lastCallback.setPersonalDataManagerListener!
      (address, [], [], manager.data.accountInfo);
  await flushTasks();
}

export function getAddressFieldValue(
    address: chrome.autofillPrivate.AddressEntry,
    type: chrome.autofillPrivate.FieldType): string|undefined {
  return address.fields.find(entry => entry.type === type)?.value;
}
