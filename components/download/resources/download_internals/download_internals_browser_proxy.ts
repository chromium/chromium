// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Contains the possible states a ServiceEntry can be in.
 */
export enum ServiceEntryState {
  NEW = 'NEW',
  AVAILABLE = 'AVAILABLE',
  ACTIVE = 'ACTIVE',
  PAUSED = 'PAUSED',
  COMPLETE = 'COMPLETE',
}

/**
 * Contains the possible states a ServiceEntry's driver can be in.
 */
export enum DriverEntryState {
  IN_PROGRESS = 'IN_PROGRESS',
  COMPLETE = 'COMPLETE',
  CANCELLED = 'CANCELLED',
  INTERRUPTED = 'INTERRUPTED',
}

/**
 * Contains the possible results a ServiceEntry can have.
 */
export enum ServiceEntryResult {
  SUCCEED = 'SUCCEED',
  FAIL = 'FAIL',
  ABORT = 'ABORT',
  TIMEOUT = 'TIMEOUT',
  UNKNOWN = 'UNKNOWN',
  CANCEL = 'CANCEL',
  OUT_OF_RETRIES = 'OUT_OF_RETRIES',
  OUT_OF_RESUMPTIONS = 'OUT_OF_RESUMPTIONS',
}

/**
 * Contains the possible results of a ServiceRequest.
 */
export enum ServiceRequestResult {
  ACCEPTED = 'ACCEPTED',
  BACKOFF = 'BACKOFF',
  UNEXPECTED_CLIENT = 'UNEXPECTED_CLIENT',
  UNEXPECTED_GUID = 'UNEXPECTED_GUID',
  CLIENT_CANCELLED = 'CLIENT_CANCELLED',
  INTERNAL_ERROR = 'INTERNAL_ERROR',
}

export interface ServiceStatus {
  serviceState: string;
  modelStatus: string;
  driverStatus: string;
  fileMonitorStatus: string;
}

export interface ServiceEntry {
  client: string;
  guid: string;
  state: ServiceEntryState;
  url: string;
  bytes_downloaded: number;
  time_downloaded: string;
  result?: ServiceEntryResult;
  driver: {
    state: DriverEntryState,
    paused: boolean,
    done: boolean,
  };
}

export interface ServiceRequest {
  client: string;
  guid: string;
  result: ServiceRequestResult;
}

export interface DownloadInternalsBrowserProxy {
  /**
   * Gets the current status of the Download Service.
   * @return A promise firing when the service status is fetched.
   */
  getServiceStatus(): Promise<ServiceStatus>;

  /**
   * Gets the current list of downloads the Download Service is aware of.
   * @return A promise firing when the list of downloads is fetched.
   */
  getServiceDownloads(): Promise<ServiceEntry[]>;

  /**
   * Starts a download with the Download Service.
   */
  startDownload(url: string): Promise<void>;
}

export class DownloadInternalsBrowserProxyImpl implements
    DownloadInternalsBrowserProxy {
  getServiceStatus() {
    return sendWithPromise('getServiceStatus');
  }

  getServiceDownloads() {
    return sendWithPromise('getServiceDownloads');
  }

  startDownload(url: string) {
    return sendWithPromise('startDownload', url);
  }

  static getInstance(): DownloadInternalsBrowserProxy {
    return instance || (instance = new DownloadInternalsBrowserProxyImpl());
  }

  static setInstance(obj: DownloadInternalsBrowserProxy) {
    instance = obj;
  }
}

let instance: DownloadInternalsBrowserProxy|null = null;
