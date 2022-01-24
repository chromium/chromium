// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriverEntryState, ServiceEntryResult, ServiceEntryState, ServiceRequestResult} from './download_internals_browser_proxy.js';

// Expose these on |window| because they need to be accessed by jstemplate code
// in download_internals.html
window.downloadInternalsVisuals = {};

function getOngoingServiceEntryClass(entry) {
  switch (entry.state) {
    case ServiceEntryState.NEW:
      return 'service-entry-new';
    case ServiceEntryState.AVAILABLE:
      return 'service-entry-available';
    case ServiceEntryState.ACTIVE:
      if (entry.driver == undefined || !entry.driver.paused ||
          entry.driver.state == DriverEntryState.INTERRUPTED) {
        return 'service-entry-active';
      } else {
        return 'service-entry-blocked';
      }
    case ServiceEntryState.PAUSED:
      return 'service-entry-paused';
    case ServiceEntryState.COMPLETE:
      return 'service-entry-success';
    default:
      return '';
  }
}
window.downloadInternalsVisuals.getOngoingServiceEntryClass =
    getOngoingServiceEntryClass;

function getFinishedServiceEntryClass(entry) {
  switch (entry.result) {
    case ServiceEntryResult.SUCCEED:
      return 'service-entry-success';
    default:
      return 'service-entry-fail';
  }
}
window.downloadInternalsVisuals.getFinishedServiceEntryClass =
    getFinishedServiceEntryClass;

function getServiceRequestClass(request) {
  switch (request.result) {
    case ServiceRequestResult.ACCEPTED:
      return 'service-entry-success';
    case ServiceRequestResult.BACKOFF:
    case ServiceRequestResult.CLIENT_CANCELLED:
      return 'service-entry-blocked';
    default:
      return 'service-entry-fail';
  }
}
window.downloadInternalsVisuals.getServiceRequestClass = getServiceRequestClass;
