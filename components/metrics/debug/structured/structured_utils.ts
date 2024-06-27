// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * Key-Value pair for the summary and metrics table.
 */
interface KeyValue {
  key: string;
  value: number|string|boolean|[number];
}

/**
 * Sequence information for an event.
 */
interface SequenceMetadata {
  id: string;
  systemUptimeMs: string;
  resetCounter: string;
}

/**
 * An event and its data. This includes metadata about the event and sequence
 * information if applicable.
 */
export interface StructuredMetricEvent {
  project: string;
  event: string;
  type: string;
  sequenceMetadata?: SequenceMetadata;
  metrics: KeyValue[];
}

/**
 * Summary about Structured Metrics service.
 */
export interface StructuredMetricsSummary {
  enabled: boolean;
  flags: KeyValue[];
  crosDeviceId: string;
}

/**
 * Contains the search parameters by category.
 *
 * Valid categories are: project, event, metric.
 */
export type SearchParams = Map<string, string>;

/**
 * Updates the Summary table with new information.
 *
 * @param summaryBody Body of the summary table.
 * @param summary Summary object to populate the table.
 * @param template Key-Value pair HTML template.
 */
export function updateStructuredMetricsSummary(
    summaryBody: HTMLElement, summary: StructuredMetricsSummary,
    template: HTMLTemplateElement): void {
  // Clear the table first.
  summaryBody.replaceChildren();

  const enabled =
      buildKeyValueRow('Enabled', summary.enabled.toString(), template);
  summaryBody.append(enabled);

  // If we do not get a value, do not display it. This value doesn't make sense
  // on some platforms.
  if (summary.crosDeviceId) {
    const crosDeviceId =
        buildKeyValueRow('CrOS Device Id', summary.crosDeviceId, template);
    summaryBody.append(crosDeviceId);
  }
}

/**
 * Updates the events table with the events recorded by the client.
 *
 * @param eventBody Body of the event table.
 * @param events List of events to populate the table.
 * @param searchParams Optional search parameters.
 * @param template HTML template for the event table row.
 * @param kvTemplate Key-Value pair HTML template.
 */
export function updateStructuredMetricsEvents(
    eventBody: HTMLElement, events: StructuredMetricEvent[],
    searchParams: SearchParams|null, eventTemplate: HTMLTemplateElement,
    detailsTemplate: HTMLTemplateElement,
    kvTemplate: HTMLTemplateElement): void {
  // If chrome://metrics-internal is opened on Windows, Mac, or Linux and
  // Structured Metrics is disabled, we should do nothing.
  if (events === null) {
    return;
  }

  eventBody.replaceChildren();

  for (const event of events) {
    // If there is a |searchParams| and the event doesn't satisfy the
    // |searchParams| then it can be skipped.
    if (searchParams !== null && !checkSearch(event, searchParams)) {
      continue;
    }

    const row = eventTemplate.content.cloneNode(true) as HTMLElement;
    const [project, evn, type, uptime] = row.querySelectorAll('td');

    assert(project);
    project.textContent = event.project;

    assert(evn);
    evn.textContent = event.event;

    assert(type);
    type.textContent = event.type;

    assert(uptime);
    uptime.textContent = event.sequenceMetadata?.systemUptimeMs ?? '-';

    const detailsRow = detailsTemplate.content.cloneNode(true) as HTMLElement;
    const metricsRow = detailsRow.querySelector<HTMLElement>('#metrics-row');
    assert(metricsRow);

    const [details, metrics] = detailsRow.querySelectorAll('tbody');
    assert(details);
    assert(metrics);

    updateEventDetailsTable(details, event, kvTemplate);
    updateEventMetricsTable(metrics, event, kvTemplate);

    const eventRow = row.querySelector('#event-row');
    assert(eventRow);
    eventRow.addEventListener('click', () => {
      if (metricsRow.style.display === 'none') {
        metricsRow.style.display = 'table-row';
      } else {
        metricsRow.style.display = 'none';
      }
    }, false);

    eventBody.append(row);
    eventBody.append(detailsRow);
  }
}

function updateEventDetailsTable(
    detailTable: HTMLElement, event: StructuredMetricEvent,
    template: HTMLTemplateElement): void {
  detailTable.replaceChildren();

  const resetCounter = event.sequenceMetadata?.resetCounter ?? '-';
  const systemUptime = event.sequenceMetadata?.systemUptimeMs ?? '-';
  const eventId = event.sequenceMetadata?.id ?? '-';

  const resetCounterRow = buildKeyValueRow('Reset Id', resetCounter, template);
  const systemUptimeRow =
      buildKeyValueRow('System Uptime', systemUptime, template);
  const eventIdRow = buildKeyValueRow('Event Id', eventId, template);

  detailTable.append(resetCounterRow);
  detailTable.append(systemUptimeRow);
  detailTable.append(eventIdRow);
}

function checkSearch(
    event: StructuredMetricEvent, searchParams: SearchParams): boolean {
  const projectSearch = searchParams.get('project');
  const eventSearch = searchParams.get('event');
  const metricSearch = searchParams.get('metric');

  if (projectSearch &&
      event.project.toLowerCase().indexOf(projectSearch.toLowerCase()) === -1) {
    return false;
  }

  if (eventSearch &&
      event.event.toLowerCase().indexOf(eventSearch.toLowerCase()) === -1) {
    return false;
  }

  if (metricSearch &&
      event.metrics.find(
          (metric: KeyValue) =>
              metric.key.toLowerCase().indexOf(metricSearch.toLowerCase()) !==
              -1) === undefined) {
    return false;
  }
  return true;
}

function updateEventMetricsTable(
    metricsTable: HTMLElement, event: StructuredMetricEvent,
    template: HTMLTemplateElement): void {
  metricsTable.replaceChildren();
  for (const metric of event.metrics) {
    const metricRow =
        buildKeyValueRow(metric.key, metric.value.toString(), template);
    metricsTable.append(metricRow);
  }
}

function buildKeyValueRow(
    key: string, value: string, template: HTMLTemplateElement): HTMLElement {
  const kvRow = template.content.cloneNode(true) as HTMLElement;

  const [k, v] = kvRow.querySelectorAll('td');
  assert(k);
  k.textContent = key;
  assert(v);
  v.textContent = value;

  return kvRow;
}
