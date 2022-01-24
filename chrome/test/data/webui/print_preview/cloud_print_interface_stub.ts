// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterface, CloudPrintInterfaceEventType, createDestinationKey, Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {getCddTemplate} from './print_preview_test_utils.js';

/**
 * Test version of the cloud print interface.
 */
export class CloudPrintInterfaceStub extends TestBrowserProxy implements
    CloudPrintInterface {
  private eventTarget_: EventTarget = new EventTarget();
  private searchInProgress_: boolean = false;
  private cloudPrintersMap_: Map<string, Destination> = new Map();
  private initialized_: boolean = false;
  private users_: string[] = [];
  private configured_: boolean = false;

  constructor() {
    super(['printer', 'search', 'submit']);
  }

  configure() {
    this.configured_ = true;
  }

  getEventTarget() {
    return this.eventTarget_;
  }

  isCloudDestinationSearchInProgress() {
    return this.searchInProgress_;
  }

  isConfigured() {
    return this.configured_;
  }

  setUsers(users: string[]) {
    this.users_ = users;
  }

  areCookieDestinationsDisabled() {
    return false;
  }

  /**
   * @param printer The destination to return when the printer is requested.
   */
  setPrinter(printer: Destination) {
    this.cloudPrintersMap_.set(printer.key, printer);
    if (!this.users_.includes(printer.account)) {
      this.users_.push(printer.account);
    }
  }

  /**
   * Dispatches a CloudPrintInterfaceEventType.SEARCH_DONE event with the
   * printers that have been set so far using setPrinter().
   */
  search(account: string) {
    this.methodCalled('search', account);
    this.searchInProgress_ = true;
    const activeUser = !!account && this.users_.includes(account) ?
        account :
        (this.users_[0] || '');
    if (activeUser) {
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.UPDATE_USERS,
          {detail: {users: this.users_, activeUser: activeUser}}));
      this.initialized_ = true;
    }

    const printers: Destination[] = [];
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

  /**
   * Dispatches a CloudPrintInterfaceEventType.PRINTER_DONE event with the
   * printer details if the printer has been added by calling setPrinter().
   */
  printer(printerId: string, origin: DestinationOrigin, account: string) {
    // Use setTimeout to make this return asynchronously to better simulate the
    // real CloudPrintInterface. This allows testing for timing issues, e.g.
    // https://crbug.com/1038645
    window.setTimeout(() => {
      this.methodCalled(
          'printer', {id: printerId, origin: origin, account: account});
      const printer = this.cloudPrintersMap_.get(
          createDestinationKey(printerId, origin, account || ''));

      if (!this.initialized_) {
        const activeUser = !!account && this.users_.includes(account) ?
            account :
            (this.users_[0] || '');
        if (activeUser) {
          this.eventTarget_.dispatchEvent(new CustomEvent(
              CloudPrintInterfaceEventType.UPDATE_USERS,
              {detail: {users: this.users_, activeUser: activeUser}}));
          this.initialized_ = true;
        }
      }
      if (printer) {
        printer.capabilities = getCddTemplate(printerId).capabilities;
        this.eventTarget_.dispatchEvent(new CustomEvent(
            CloudPrintInterfaceEventType.PRINTER_DONE, {detail: printer}));
      } else {
        this.eventTarget_.dispatchEvent(
            new CustomEvent(CloudPrintInterfaceEventType.PRINTER_FAILED, {
              detail: {
                account: account,
                origin: origin,
                destinationId: printerId,
                status: 200,
                message: 'Unknown printer',
              },
            }));
      }
    }, 1);
  }

  submit(
      destination: Destination, printTicket: string, documentTitle: string,
      data: string) {
    this.methodCalled('submit', {
      destination: destination,
      printTicket: printTicket,
      documentTitle: documentTitle,
      data: data
    });
  }
}
