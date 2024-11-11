// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ServiceEntry, ServiceRequest} from './download_internals_browser_proxy.js';
import {DriverEntryState, ServiceEntryResult, ServiceEntryState, ServiceRequestResult} from './download_internals_browser_proxy.js';

export function getOngoingServiceEntryClass(entry: ServiceEntry) {
  switch (entry.state) {
    case ServiceEntryState.NEW:
      return 'service-entry-new';
    case ServiceEntryState.AVAILABLE:
      return 'service-entry-available';
    case ServiceEntryState.ACTIVE:
      if (entry.driver == null || !entry.driver.paused ||
          entry.driver.state === DriverEntryState.INTERRUPTED) {
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

export function getFinishedServiceEntryClass(entry: ServiceEntry) {
  switch (entry.result) {
    case ServiceEntryResult.SUCCEED:
      return 'service-entry-success';
    default:
      return 'service-entry-fail';
  }
}

export function getServiceRequestClass(request: ServiceRequest) {
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
