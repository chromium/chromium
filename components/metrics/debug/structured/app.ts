// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './app.html.js';
import {StructuredMetricsBrowserProxyImpl} from './structured_metrics_browser_proxy.js';
import type {StructuredMetricEvent, StructuredMetricsSummary} from './structured_utils.js';
import {updateStructuredMetricsEvents, updateStructuredMetricsSummary} from './structured_utils.js';

export class StructuredMetricsInternalsAppElement extends CustomElement {
  static get is(): string {
    return 'structured-metrics-internals-app';
  }

  static override get template() {
    return getTemplate();
  }

  private browserProxy_: StructuredMetricsBrowserProxyImpl =
      StructuredMetricsBrowserProxyImpl.getInstance();
  private summaryIntervalId_: ReturnType<typeof setInterval>;

  initPromise: Promise<void>;

  constructor() {
    super();
    this.initPromise = this.init_();

    // Create periodic callbacks.
    this.summaryIntervalId_ =
        setInterval(() => this.updateStructuredMetricsSummary_(), 5000);
  }

  disconnectedCallback() {
    clearInterval(this.summaryIntervalId_);
  }

  private async init_(): Promise<void> {
    // Fetch Structured Metrics summary and events.
    // TODO: Implement a push model as new events are recorded.
    await this.updateStructuredMetricsSummary_();
    await this.updateStructuredMetricsEvents_();

    const eventRefreshButton = this.getRequiredElement('#sm-refresh-events');
    eventRefreshButton.addEventListener(
        'click', () => this.updateStructuredMetricsEvents_());
  }

  /**
   * Fetches summary information of the Structured Metrics service and renders
   * it.
   */
  private async updateStructuredMetricsSummary_(): Promise<void> {
    const summary: StructuredMetricsSummary =
        await this.browserProxy_.fetchStructuredMetricsSummary();
    const template =
        this.getRequiredElement<HTMLTemplateElement>('#summary-row-template');
    const smSummaryBody = this.getRequiredElement('#sm-summary-body');
    updateStructuredMetricsSummary(smSummaryBody, summary, template);
  }

  /**
   * Fetches all events currently recorded by the Structured Metrics Service and
   * renders them. It an event has been uploaded then it will not be shown
   * again. This only shows Events recorded in Chromium. Platform2 events are
   * not supported yet.
   */
  private async updateStructuredMetricsEvents_(): Promise<void> {
    const events: StructuredMetricEvent[] =
        await this.browserProxy_.fetchStructuredMetricsEvents();
    const eventTemplate = this.getRequiredElement<HTMLTemplateElement>(
        '#structured-metrics-event-row-template');
    const eventDetailsTemplate = this.getRequiredElement<HTMLTemplateElement>(
        '#structured-metrics-event-details-template');

    const kvTemplate =
        this.getRequiredElement<HTMLTemplateElement>('#summary-row-template');
    const eventTableBody = this.getRequiredElement('#sm-events-body');

    updateStructuredMetricsEvents(
        eventTableBody, events, eventTemplate, eventDetailsTemplate,
        kvTemplate);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'structured-metrics-internals-app': StructuredMetricsInternalsAppElement;
  }
}

customElements.define(
    StructuredMetricsInternalsAppElement.is,
    StructuredMetricsInternalsAppElement);
