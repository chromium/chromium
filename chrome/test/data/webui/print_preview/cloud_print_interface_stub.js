// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceEventType, createDestinationKey, DestinationOrigin} from 'chrome://print/print_preview.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {getCddTemplate} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

/**
 * Test version of the cloud print interface.
 * @implements {CloudPrintInterface}
 */
export class CloudPrintInterfaceStub extends TestBrowserProxy {
  constructor() {
    super(['printer', 'search', 'submit']);

    /** @private {!EventTarget} */
    this.eventTarget_ = new EventTarget();

    /** @private {boolean} */
    this.searchInProgress_ = false;

    /** @private {!Map<string, !Destination>} */
    this.cloudPrintersMap_ = new Map();

    /** @private {boolean} */
    this.initialized_ = false;

    /** @private {!Array<string>} */
    this.users_ = [];
  }

  /** @override */
  getEventTarget() {
    return this.eventTarget_;
  }

  /** @override */
  isCloudDestinationSearchInProgress() {
    return this.searchInProgress_;
  }

  /** @override */
  setUsers(users) {
    this.users_ = users;
  }

  /**
   * @param {!Destination} printer The destination to return
   *     when the printer is requested.
   */
  setPrinter(printer) {
    this.cloudPrintersMap_.set(printer.key, printer);
    if (!this.users_.includes(printer.account)) {
      this.users_.push(printer.account);
    }
  }

  /**
   * Helper method to derive logged in users from the |cloudPrintersMap_|.
   * @return {!Array<string>} The logged in user accounts.
   */
  getUsers_() {
    const users = [];
    this.cloudPrintersMap_.forEach((printer, key) => {
      if (!users.includes(printer.account)) {
        users.push(printer.account);
      }
    });
    return users;
  }

  /**
   * Dispatches a CloudPrintInterfaceEventType.SEARCH_DONE event with the
   * printers that have been set so far using setPrinter().
   * @override
   */
  search(account) {
    this.methodCalled('search', account);
    this.searchInProgress_ = true;
    const activeUser =
        this.users_.includes(account) ? account : (this.users_[0] || '');
    if (activeUser) {
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.UPDATE_USERS,
          {detail: {users: this.users_, activeUser: activeUser}}));
      this.initialized_ = true;
    }

    const printers = [];
    this.cloudPrintersMap_.forEach((value) => {
      if (value.account === account) {
        printers.push(value);
      }
    });

    const searchDoneEvent =
        new CustomEvent(CloudPrintInterfaceEventType.SEARCH_DONE, {
          detail: {
            origin: DestinationOrigin.COOKIES,
            printers: printers,
            isRecent: true,
            user: account,
            searchDone: true,
          }
        });
    this.searchInProgress_ = false;
    this.eventTarget_.dispatchEvent(searchDoneEvent);
  }

  /** @override */
  invites(account) {}

  /**
   * Dispatches a CloudPrintInterfaceEventType.PRINTER_DONE event with the
   * printer details if the printer has been added by calling setPrinter().
   * @override
   */
  printer(printerId, origin, account) {
    this.methodCalled(
        'printer', {id: printerId, origin: origin, account: account});
    const printer = this.cloudPrintersMap_.get(
        createDestinationKey(printerId, origin, account));

    if (!this.initialized_) {
      const activeUser =
          this.users_.includes(account) ? account : (this.users_[0] || '');
      if (activeUser) {
        this.eventTarget_.dispatchEvent(new CustomEvent(
            CloudPrintInterfaceEventType.UPDATE_USERS,
            {detail: {users: this.users_, activeUser: activeUser}}));
        this.initialized_ = true;
      }
    }
    if (printer) {
      printer.capabilities = getCddTemplate(printerId);
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.PRINTER_DONE, {detail: printer}));
    } else {
      this.eventTarget_.dispatchEvent(
          new CustomEvent(CloudPrintInterfaceEventType.PRINTER_FAILED, {
            detail: {
              origin: origin,
              destinationId: printerId,
              status: 200,
              message: 'Unknown printer',
            },
          }));
    }
  }

  submit(destination, printTicket, documentTitle, data) {
    this.methodCalled('submit', {
      destination: destination,
      printTicket: printTicket,
      documentTitle: documentTitle,
      data: data
    });
  }
}
