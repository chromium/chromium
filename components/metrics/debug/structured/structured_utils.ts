// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

/**
 * Key-Value pair for the summary and metrics table.
 */
interface KeyValue {
  key: string;
  value: number|string|boolean;
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

  const crosDeviceId =
      buildKeyValueRow('CrOS Device Id', summary.crosDeviceId || '-', template);
  summaryBody.append(crosDeviceId);
}

/**
 * Updates the events table with the events recorded by the client.
 *
 * @param eventBody Body of the event table.
 * @param events List of events to populate the table.
 * @param template HTML template for the event table row.
 * @param kvTemplate Key-Value pair HTML template.
 */
export function updateStructuredMetricsEvents(
    eventBody: HTMLElement, events: StructuredMetricEvent[],
    eventTemplate: HTMLTemplateElement, detailsTemplate: HTMLTemplateElement,
    kvTemplate: HTMLTemplateElement): void {
  eventBody.replaceChildren();

  for (const event of events) {
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
    const metricsRow = detailsRow.querySelector('#metrics-row') as HTMLElement;
    assert(metricsRow);

    const [details, metrics] = detailsRow.querySelectorAll('tbody');
    assert(details);
    assert(metrics);

    updateEventDetailsTable(details, event, kvTemplate);
    updateEventMetricsTable(metrics, event, kvTemplate);

    const eventRow = row.querySelector('#event-row') as HTMLElement;
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
