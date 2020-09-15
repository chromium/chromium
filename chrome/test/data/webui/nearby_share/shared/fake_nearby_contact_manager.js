// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of ContactManagerInterface for testing.
 */
cr.define('nearby_share', function() {
  /**
   * Fake implementation of nearbyShare.mojom.ContactManagerInterface
   *
   * @implements {nearbyShare.mojom.ContactManagerInterface}
   */
  /* #export */ class FakeContactManager {
    constructor() {
      /** @type {?Array<!nearbyShare.mojom.ContactRecord>} */
      this.contactRecords = null;
      /** @private {!Array<!string>} */
      this.allowedContacts_ = [];
      /** @private {?nearbyShare.mojom.DownloadContactsObserverInterface} */
      this.observer_;
      /** @private {Object} */
      this.$ = {
        close() {},
      };
    }

    /**
     * @param {!nearbyShare.mojom.DownloadContactsObserverInterface} observer
     */
    addDownloadContactsObserver(observer) {
      // Just support a single observer for testing.
      this.observer_ = observer;
    }

    downloadContacts() {
      // This does nothing intentionally, call failDownload() or
      // completeDownload() to simulate a response.
    }

    /**
     * @param {!Array<!string>} allowedContacts
     */
    setAllowedContacts(allowedContacts) {
      this.allowedContacts_ = allowedContacts;
    }

    setupContactRecords() {
      this.contactRecords = [
        {
          id: '1',
          personName: 'Jane Doe',
          identifiers: [
            {accountName: 'jane@gmail.com'},
            {phoneNumber: '555-1212'},
          ]
        },
        {
          id: '2',
          personName: 'John Smith',
          identifiers: [
            {phoneNumber: '555-5555'},
            {accountName: 'smith@google.com'},
          ]
        }
      ];
      this.allowedContacts_ = ['1'];
    }

    failDownload() {
      this.observer_.onContactsDownloadFailed();
    }

    completeDownload() {
      this.observer_.onContactsDownloaded(
          this.allowedContacts_, this.contactRecords || []);
    }
  }
  // #cr_define_end
  return {FakeContactManager: FakeContactManager};
});
