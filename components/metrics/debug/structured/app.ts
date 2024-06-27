// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './app.html.js';
import {StructuredMetricsBrowserProxyImpl} from './structured_metrics_browser_proxy.js';
import type {SearchParams, StructuredMetricEvent, StructuredMetricsSummary} from './structured_utils.js';
import {updateStructuredMetricsEvents, updateStructuredMetricsSummary} from './structured_utils.js';

/**
 * Gets search params from search url.
 */
function getSearchParams(): string {
  return window.location.search.substring(1);
}

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

  textSearch: HTMLInputElement;

  searchQuery: SearchParams|null = null;

  searchError: boolean = false;

  constructor() {
    super();

    // This must be set before |initSearchParams_| is called.
    this.textSearch = this.getRequiredElement<HTMLInputElement>('#search-bar');
    this.initSearchParams_();

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
        eventTableBody, events, this.searchQuery, eventTemplate,
        eventDetailsTemplate, kvTemplate);
  }

  /**
   * Initializes search params from the URL.
   */
  private initSearchParams_() {
    const searchString = this.sanitizeUrlToSearch_();
    this.searchQuery = this.parseSearchString_(searchString);

    this.textSearch.value = searchString;
    this.textSearch.addEventListener('search', () => {
      this.updateSearchCriteria_();
    });
  }


  /**
   * Updates the windows search url.
   */
  private updateSearchCriteria_() {
    // Update the url to reflect the search string. This will redirect the new
    // url page, updating the searchQuery.
    window.location.search = '?' + this.sanitizeSearchToUrl_();
  }

  /**
   * Sanitize the search format into a valid format for the URL.
   */
  private sanitizeSearchToUrl_(): string {
    return this.textSearch.value.replace(/\s+/gi, '&').replace(/:/gi, '=');
  }

  /**
   * Sanitize the URL search parameters into the search format.
   */
  private sanitizeUrlToSearch_(): string {
    return getSearchParams().replace(/&/gi, ' ').replace(/=/gi, ':');
  }

  /**
   * Parse search format into an object.
   *
   * The format is a space separated lists of "key:value" pairs. Currently, a
   * single search term is not supported.
   */
  private parseSearchString_(text: string): SearchParams|null {
    // Page should be rebuilt on a new search query, but leaving it because it
    // is better to be safe then have an error message that doesn't disappear
    // when the page is refreshed..
    if (this.searchError) {
      this.hideSearchErrorMessage_();
    }

    if (text.length == 0) {
      return null;
    }

    // If an ':' is not found then we are doing a full text search. The string
    // is the query as is.
    if (text.indexOf(':') == -1) {
      return null;
    }

    // If it is found, then we are doing a categorical search, this parses the
    // string into a map of category and search value.
    const categories = new Map<string, string>();
    const validSearchKeys = ['project', 'event', 'metric'];

    text.split(' ').forEach((cat) => {
      const [key, value] = cat.split(':', 2);
      if (key !== undefined && value !== undefined) {
        if (validSearchKeys.find((value) => value === key) === undefined) {
          this.setSearchErrorMessage_(`invalid search key: ${
              key}. valid keys are project, event, metric`);
          return;
        }
        categories.set(key, value);
      }
    });

    return categories;
  }

  /**
   * Hides the search error message.
   */
  private hideSearchErrorMessage_(): void {
    this.searchError = false;

    const errorMsg =
        this.getRequiredElement<HTMLParagraphElement>('#search-error-msg');
    assert(errorMsg);
    errorMsg.style.display = 'none';
  }

  /**
   * Sets and shows the error message.
   */
  private setSearchErrorMessage_(msg: string): void {
    this.searchError = true;

    // Set the content of the error message.
    const errorMsg =
        this.getRequiredElement<HTMLParagraphElement>('#search-error-msg');
    assert(errorMsg);
    errorMsg.innerText = msg;

    // Shows the error message
    errorMsg.style.display = 'block';
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
