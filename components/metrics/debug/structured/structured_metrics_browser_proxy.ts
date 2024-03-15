// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the
 * chrome://metrics-internals/structured page to interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.js';

import type {StructuredMetricEvent, StructuredMetricsSummary} from './structured_utils.js';

export interface StructuredMetricsBrowserProxy {
  /**
   * Fetches recorded events from Structured Metrics Service.
   */
  fetchStructuredMetricsEvents(): Promise<StructuredMetricEvent[]>;

  /**
   * Fetches a summary of the Structured Metrics Service.
   */
  fetchStructuredMetricsSummary(): Promise<StructuredMetricsSummary>;
}

export class StructuredMetricsBrowserProxyImpl implements
    StructuredMetricsBrowserProxy {
  fetchStructuredMetricsEvents() {
    return sendWithPromise('fetchStructuredMetricsEvents');
  }

  fetchStructuredMetricsSummary() {
    return sendWithPromise('fetchStructuredMetricsSummary');
  }

  static getInstance() {
    return instance || (instance = new StructuredMetricsBrowserProxyImpl());
  }
}

let instance: StructuredMetricsBrowserProxy|null = null;
