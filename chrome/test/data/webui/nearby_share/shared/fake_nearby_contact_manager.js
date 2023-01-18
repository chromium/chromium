// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of ContactManagerInterface for testing.
 */

import {ContactManagerInterface, ContactRecord, DownloadContactsObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-webui.js';

/**
 * Fake implementation of ContactManagerInterface
 *
 * @implements {ContactManagerInterface}
 */
export class FakeContactManager {
  constructor() {
    /** @type {?Array<!ContactRecord>} */
    this.contactRecords = null;
    /** @type {!Array<!string>} */
    this.allowedContacts = [];
    /** @private {number} */
    this.numUnreachable_ = 3;
    /** @private {?DownloadContactsObserverInterface} */
    this.observer_;
    /** @private {Object} */
    this.$ = {
      close() {},
    };
    /** @type {boolean} */
    this.downloadContactsCalled = false;
  }

  /**
   * @param {!DownloadContactsObserverInterface} observer
   */
  addDownloadContactsObserver(observer) {
    // Just support a single observer for testing.
    this.observer_ = observer;
  }

  downloadContacts() {
    // This does nothing intentionally, call failDownload() or
    // completeDownload() to simulate a response.
    this.downloadContactsCalled = true;
  }

  /**
   * @param {!Array<!string>} allowedContacts
   */
  setAllowedContacts(allowedContacts) {
    this.allowedContacts = allowedContacts;
  }

  /**
   * @param {number} numUnreachable
   */
  setNumUnreachable(numUnreachable) {
    this.numUnreachable_ = numUnreachable;
  }

  setupContactRecords() {
    this.contactRecords = [
      {
        id: '1',
        personName: 'Jane Doe',
        identifiers: [
          {accountName: 'jane@gmail.com'},
          {phoneNumber: '555-1212'},
        ],
      },
      {
        id: '2',
        personName: 'John Smith',
        identifiers: [
          {phoneNumber: '555-5555'},
          {accountName: 'smith@google.com'},
        ],
      },
    ];
    this.allowedContacts = ['1'];
  }

  failDownload() {
    this.observer_.onContactsDownloadFailed();
  }

  completeDownload() {
    this.observer_.onContactsDownloaded(
        this.allowedContacts, this.contactRecords || [],
        /*num_unreachable_contacts_filtered_out=*/ this.numUnreachable_);
  }
}
