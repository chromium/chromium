// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://browser-actuator-internals/app.js';

import type {BrowserActuatorInternalsAppElement} from 'chrome://browser-actuator-internals/app.js';
import {browserProxyFactory, MessageDirection} from 'chrome://browser-actuator-internals/browser_actuator_internals.mojom-webui.js';
import type {RecordedEvent, SessionSummary} from 'chrome://browser-actuator-internals/browser_actuator_internals.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserActuatorInternalsUiHandler} from './test_browser_actuator_internals_ui_handler.js';

function makeEvent(overrides: Partial<RecordedEvent> = {}): RecordedEvent {
  return {
    timestamp: new Date('2026-09-22T01:00:00.000Z'),
    direction: MessageDirection.kDownstream,
    payloadTypes: ['payload.TypeA'],
    message: '{}',
    messageTruncated: false,
    ...overrides,
  };
}

function makeSession(overrides: Partial<SessionSummary> = {}): SessionSummary {
  return {
    sessionId: 'session-1',
    startWallTime: new Date('2026-09-22T01:00:00.000Z'),
    endWallTime: null,
    totalDownstreamMessages: 1,
    totalUpstreamMessages: 0,
    totalEvents: 1,
    events: [makeEvent()],
    ...overrides,
  };
}

suite('BrowserActuatorInternalsAppTest', () => {
  let app: BrowserActuatorInternalsAppElement;
  let handler: TestBrowserActuatorInternalsUiHandler;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = new TestBrowserActuatorInternalsUiHandler();
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
  });

  async function render(sessions: SessionSummary[] = []):
      Promise<BrowserActuatorInternalsAppElement> {
    handler.setSessions(sessions);
    app = document.createElement('browser-actuator-internals-app');
    document.body.appendChild(app);
    await handler.whenCalled('getSessionHistory');
    await microtasksFinished();
    return app;
  }

  function getText(parent: Element|ShadowRoot, selector: string): string {
    const element = parent.querySelector<HTMLElement>(selector);
    assertTrue(!!element, `Missing element: ${selector}`);
    return element.textContent.trim().replace(/\s+/g, ' ');
  }

  test('LoadsOnceAndRendersCards', async () => {
    await render([
      makeSession({
        sessionId: 'session-act',
        totalDownstreamMessages: 10,
        totalUpstreamMessages: 5,
      }),
      makeSession({
        sessionId: 'session-cls',
        endWallTime: new Date('2026-09-22T02:00:00.000Z'),
        totalDownstreamMessages: 20,
        totalUpstreamMessages: 15,
        events: [],
      }),
    ]);

    assertEquals(1, handler.getCallCount('getSessionHistory'));
    const cards = app.shadowRoot.querySelectorAll('.session-card');
    assertEquals(2, cards.length);

    // Active session
    const card1 = cards[0]!;
    assertTrue(getText(card1, '.session-id').includes('session-act'));
    assertEquals('Active', getText(card1, '.badge'));
    assertTrue(card1.querySelector('.badge')!.classList.contains('active'));
    assertEquals('10', getText(card1, '.downstream-count'));
    assertEquals('5', getText(card1, '.upstream-count'));
    assertEquals(null, card1.querySelector('.end-time'));
    assertTrue(getText(card1, '.stat-grid').includes('—'));

    // Closed session (with empty events)
    const card2 = cards[1]!;
    assertTrue(getText(card2, '.session-id').includes('session-cls'));
    assertEquals('Closed', getText(card2, '.badge'));
    assertTrue(card2.querySelector('.badge')!.classList.contains('closed'));
    assertEquals('20', getText(card2, '.downstream-count'));
    assertEquals('15', getText(card2, '.upstream-count'));
    assertEquals('2026-09-22T02:00:00.000Z', getText(card2, '.end-time'));
    assertEquals(null, card2.querySelector('details.events'));
  });

  test('EventsDetailsAndRows', async () => {
    const events = [
      makeEvent(
          {direction: MessageDirection.kDownstream, payloadTypes: ['TypeA']}),
      makeEvent({
        direction: MessageDirection.kUpstream,
        payloadTypes: ['TypeB', 'TypeC'],
      }),
    ];
    await render([makeSession({events, totalEvents: 2})]);

    const card = app.shadowRoot.querySelector('.session-card')!;
    const details = card.querySelector<HTMLDetailsElement>('details.events')!;
    assertFalse(details.open);

    const rows = card.querySelectorAll('.event-row');
    assertEquals(2, rows.length);
    assertEquals('Downstream', getText(rows[0]!, '.event-direction'));
    assertEquals('TypeA', getText(rows[0]!, '.event-payload-types'));
    assertEquals('Upstream', getText(rows[1]!, '.event-direction'));
    assertEquals('TypeB, TypeC', getText(rows[1]!, '.event-payload-types'));

    const content = app.shadowRoot.textContent || '';
    assertFalse(content.includes('↓'));
    assertFalse(content.includes('↑'));
  });

  test('EventsSummaryCappedAndUncapped', async () => {
    await render([
      makeSession({totalEvents: 100}),
      makeSession({totalEvents: 1}),
    ]);

    const summaries =
        app.shadowRoot.querySelectorAll<HTMLElement>('.events-summary');
    assertEquals(2, summaries.length);
    assertEquals('Events (showing 1 of 100)', summaries[0]!.textContent.trim());
    assertEquals('Events (1)', summaries[1]!.textContent.trim());
  });

  test('MessageTextRendersAsLiteralText', async () => {
    await render([makeSession({events: [makeEvent({message: '<b>x</b>'})]})]);

    const card = app.shadowRoot.querySelector('.session-card')!;
    assertEquals('<b>x</b>', getText(card, '.message-text'));
    assertEquals(0, card.querySelectorAll('b').length);
  });

  test('MessageTruncationAndCollapsiblePreview', async () => {
    const longMsg = 'A'.repeat(81);
    const shortMsg = 'B'.repeat(80);
    const events = [
      makeEvent({message: longMsg, messageTruncated: true}),
      makeEvent({message: longMsg, messageTruncated: false}),
      makeEvent({message: shortMsg, messageTruncated: true}),
    ];
    await render([makeSession({events, totalEvents: 3})]);

    const rows = app.shadowRoot.querySelectorAll('.event-row');
    assertEquals(3, rows.length);

    // 1. Long truncated (81 chars): collapsible, "(truncated at 8 KB)", and
    // note
    const row0 = rows[0]!;
    const details0 =
        row0.querySelector<HTMLDetailsElement>('details.message-details')!;
    assertFalse(details0.open);
    assertEquals(longMsg.slice(0, 80) + '...', getText(row0, '.message-text'));
    assertEquals('(truncated at 8 KB)', getText(row0, '.truncated-marker'));
    assertEquals(longMsg, getText(row0, '.message-full'));
    assertTrue(getText(row0, '.truncated-note').includes('at 8 KB'));

    // 2. Long non-truncated (81 chars): collapsible preview, no marker or note
    const row1 = rows[1]!;
    const details1 =
        row1.querySelector<HTMLDetailsElement>('details.message-details')!;
    assertFalse(details1.open);
    assertEquals(longMsg.slice(0, 80) + '...', getText(row1, '.message-text'));
    assertEquals(null, row1.querySelector('.truncated-marker'));
    assertEquals(longMsg, getText(row1, '.message-full'));
    assertEquals(null, row1.querySelector('.truncated-note'));

    // 3. Short truncated (80 chars): not collapsible, inline marker, no note
    const row2 = rows[2]!;
    assertEquals(null, row2.querySelector('details.message-details'));
    assertEquals(shortMsg, getText(row2, '.message-text'));
    assertEquals('(truncated at 8 KB)', getText(row2, '.truncated-marker'));
    assertEquals(null, row2.querySelector('.truncated-note'));
  });

  test('EmptyStateAndNoLegacyConnection', async () => {
    await render([]);

    assertEquals(
        'No sessions yet. Recording starts when Chrome starts with the ' +
            'BrowserActuatorInternals feature enabled.',
        getText(app.shadowRoot, '#empty-state'));
    assertEquals(
        'Total Sessions: 0', getText(app.shadowRoot, '.session-count'));
    assertEquals(0, app.shadowRoot.querySelectorAll('.session-card').length);

    const content = app.shadowRoot.textContent || '';
    assertFalse(content.includes('Disconnected'));
    assertFalse(content.includes('Downstream Transport Connection'));
    assertEquals(null, app.shadowRoot.querySelector('.badge.disconnected'));
  });
});
