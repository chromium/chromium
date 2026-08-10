// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsAddressEditDialogElement, SettingsAddressRemoveConfirmationDialogElement, SettingsContactInfoPageElement} from 'chrome://settings/lazy_load.js';
import {AutofillManagerImpl} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createAddressEntry, TestAutofillManager} from './autofill_fake_data.js';
// clang-format on

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
 * Creates the contact info page for the given list.
 *
 * When @accountInfo is provided, it is set on the autofill manager. The value
 * `null` removes the accountInfo on the autofill manager property. The value
 * `undefined` doesn't set or change the accountInfo on the autofill manager
 * property.
 */
export async function createContactInfoPage(
    addresses: chrome.autofillPrivate.AddressEntry[],
    prefValues: Record<string, unknown>,
    accountInfo?: chrome.autofillPrivate.AccountInfo|
    null): Promise<SettingsContactInfoPageElement> {
  // Override the AutofillManagerImpl for testing.
  const autofillManager = new TestAutofillManager();
  autofillManager.data.addresses = addresses;
  if (accountInfo !== undefined) {
    autofillManager.data.accountInfo = accountInfo ?? undefined;
  }
  AutofillManagerImpl.setInstance(autofillManager);

  const page = document.createElement('settings-contact-info-page');
  page.prefs = {
    autofill: {
      profile_enabled: {
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      email_verification_state: {
        type: chrome.settingsPrivate.PrefType.DICTIONARY,
        value: {},
      },
      ...prefValues,
    },
  };
  document.body.appendChild(page);
  await autofillManager.whenCalled('getAddressList');

  return page;
}

/**
 * Creates the Edit Address dialog and fulfills the promise when the dialog
 * has actually opened.
 */
export async function createAddressDialog(
    address: chrome.autofillPrivate.AddressEntry,
    accountInfo?: chrome.autofillPrivate.AccountInfo):
    Promise<SettingsAddressEditDialogElement> {
  const dialog = document.createElement('settings-address-edit-dialog');
  dialog.address = address;
  dialog.accountInfo = accountInfo;
  document.body.appendChild(dialog);
  await eventToPromise('on-update-address-wrapper', dialog);
  return dialog;
}

export async function openAddressDialog(
    page: SettingsContactInfoPageElement):
    Promise<SettingsAddressEditDialogElement> {
  let dialog =
      page.shadowRoot!.querySelector('settings-address-edit-dialog');
  assertFalse(!!dialog, 'stale dialog found');

  page.$.addAddress.click();

  flush();

  dialog = page.shadowRoot!.querySelector('settings-address-edit-dialog');

  assertTrue(!!dialog, 'the dialog element should be in the page subtree');

  await eventToPromise('on-update-address-wrapper', dialog);
  return dialog;
}

/**
 * Opens and returns the address edit dialog element for specified
 * by |index| address in the |page| list.
 */
export async function initiateEditing(
    page: SettingsContactInfoPageElement,
    index: number): Promise<SettingsAddressEditDialogElement> {
  let dialog =
      page.shadowRoot!.querySelector<SettingsAddressEditDialogElement>(
          'settings-address-edit-dialog');
  assertFalse(!!dialog, 'stale dialog found');

  const addressElements = page.$.addressList.children;

  assertGT(
      addressElements.length, index,
      'index is too high, not enough addresses in the list');

  const menu =
      addressElements[index]!.querySelector<HTMLElement>('.address-menu');

  assertTrue(!!menu, 'the row element should contain the menu element');

  // Open menu and click the Edit button.
  menu.click();
  // Wait for the menu's items to render.
  flush();

  // Find and click the Edit button.
  const editButton =
      page.shadowRoot!.querySelector<HTMLElement>('#menuEditAddress');
  assertTrue(!!editButton, 'Edit button not found');
  editButton.click();

  flush();

  dialog = page.shadowRoot!.querySelector<SettingsAddressEditDialogElement>(
      'settings-address-edit-dialog');

  assertTrue(!!dialog, 'the dialog element should be in the page subtree');

  await eventToPromise('on-update-address-wrapper', dialog);
  return dialog;
}

/**
 * Opens and returns the remove confirmation dialog element for specified
 * by |index| address in the |page| list.
 */
export function initiateRemoving(
    page: SettingsContactInfoPageElement,
    index: number): SettingsAddressRemoveConfirmationDialogElement {
  let dialog =
      page.shadowRoot!
          .querySelector<SettingsAddressRemoveConfirmationDialogElement>(
              'settings-address-remove-confirmation-dialog');
  assertFalse(!!dialog, 'stale dialog found');

  const addressElements = page.$.addressList.children;

  assertGT(
      addressElements.length, index,
      'index is too high, not enough addresses in the list');

  const menu =
      addressElements[index]!.querySelector<HTMLElement>('.address-menu');

  assertTrue(!!menu, 'the row element should contain the menu element');

  // Open menu and click the Delete button.
  menu.click();
  flush();
  const removeButton =
      page.shadowRoot!.querySelector<HTMLElement>('#menuRemoveAddress');
  assertTrue(!!removeButton, 'Remove button not found');
  removeButton.click();

  flush();

  dialog = page.shadowRoot!
               .querySelector<SettingsAddressRemoveConfirmationDialogElement>(
                   'settings-address-remove-confirmation-dialog');

  assertTrue(!!dialog, 'the dialog element should be in the page subtree');

  return dialog;
}

/**
 * Creates the remove address dialog. Simulate clicking "Remove" button in
 * contact info page.
 */
export async function createRemoveAddressDialog(
    autofillManager: TestAutofillManager):
    Promise<SettingsAddressRemoveConfirmationDialogElement> {
  const address = createAddressEntry();
  address.metadata!.recordType =
      chrome.autofillPrivate.AddressRecordType.ACCOUNT;

  // Override the AutofillManagerImpl for testing.
  autofillManager.data.addresses = [address];
  AutofillManagerImpl.setInstance(autofillManager);

  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const page = document.createElement('settings-contact-info-page');
  document.body.appendChild(page);
  await flushTasks();

  return initiateRemoving(page, 0);
}

/**
 * Performs some UI and manager manipulations to simulate the address removal.
 */
export async function deleteAddress(
    page: SettingsContactInfoPageElement, manager: TestAutofillManager,
    index: number) {
  const dialog = await initiateRemoving(page, index);
  const closePromise = eventToPromise('close', dialog.$.dialog);
  dialog.$.remove.click();
  await closePromise;

  const address = [...manager.data.addresses];
  address.splice(index, 1);
  manager.data.addresses = address;
  manager.lastCallback.setPersonalDataManagerListener!
      (address, [], [], [], manager.data.accountInfo);
  await flushTasks();
}

export function getAddressFieldValue(
    address: chrome.autofillPrivate.AddressEntry,
    type: chrome.autofillPrivate.FieldType): string|undefined {
  return address.fields.find(entry => entry.type === type)?.value;
}
