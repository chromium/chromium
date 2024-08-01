// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSession, ForeignSessionTab, ForeignSessionWindow, HistoryAppElement, HistoryEntry, HistoryQuery} from 'chrome://history/history.js';
import type {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {middleOfNode} from 'chrome://webui-test/mouse_mock_interactions.js';


/**
 * Create a fake history result with the given timestamp.
 * @param timestamp Timestamp of the entry, as a number in ms or a string which
 *     can be parsed by Date.parse().
 * @param urlStr The URL to set on this entry.
 */
export function createHistoryEntry(
    timestamp: number|string, urlStr: string): HistoryEntry {
  if (typeof timestamp === 'string') {
    timestamp += ' UTC';
  }

  const d = new Date(timestamp);
  const url = new URL(urlStr);
  const domain = url.host;
  return {
    allTimestamps: [d.getTime()],
    remoteIconUrlForUma: '',
    isUrlInRemoteUserData: false,
    blockedVisit: false,

    // Formatting the relative day is too hard, will instead display
    // YYYY-MM-DD.
    dateRelativeDay: d.toISOString().split('T')[0]!,
    dateShort: '',
    dateTimeOfDay: d.getUTCHours() + ':' + d.getUTCMinutes(),
    deviceName: '',
    deviceType: '',
    domain: domain,
    fallbackFaviconText: '',
    hostFilteringBehavior: 0,
    readableTimestamp: '',
    selected: false,
    snippet: '',
    starred: false,
    time: d.getTime(),
    title: urlStr,
    url: urlStr,
  };
}

/**
 * Create a fake history search result with the given timestamp. Replaces fields
 * from createHistoryEntry to look like a search result.
 * @param timestamp Timestamp of the entry, as a number in ms or a string which
 *     can be parsed by Date.parse().
 * @param urlStr The URL to set on this entry.
 */
export function createSearchEntry(
    timestamp: number|string, urlStr: string): HistoryEntry {
  const entry = createHistoryEntry(timestamp, urlStr);
  entry.dateShort = entry.dateRelativeDay;
  entry.dateTimeOfDay = '';
  entry.dateRelativeDay = '';

  return entry;
}

/**
 * Create a simple HistoryQuery.
 * @param searchTerm The search term that the info has. Will be empty  string if
 *     not specified.
 */
export function createHistoryInfo(searchTerm?: string): HistoryQuery {
  return {finished: true, term: searchTerm || ''};
}

export function polymerSelectAll(element: Element, selector: string): NodeList {
  return element.shadowRoot!.querySelectorAll(selector);
}

/**
 * Returns a promise which is resolved when |eventName| is fired on |element|
 * and |predicate| is true.
 */
export function waitForEvent(
    element: HTMLElement|Window, eventName: string,
    predicate?: (e: Event) => boolean): Promise<void> {
  if (!predicate) {
    predicate = () => true;
  }

  return new Promise<void>(function(resolve) {
    const listener = function(e: Event) {
      if (!predicate!(e)) {
        return;
      }

      resolve();
      element.removeEventListener(eventName, listener);
    };

    element.addEventListener(eventName, listener);
  });
}

/**
 * Sends a shift click event to |element|.
 */
export async function shiftClick(element: CrLitElement): Promise<void> {
  const xy = middleOfNode(element);
  const props = {
    bubbles: true,
    cancelable: true,
    clientX: xy.x,
    clientY: xy.y,
    buttons: 1,
    shiftKey: true,
  };
  element.dispatchEvent(new MouseEvent('mousedown', props));
  element.dispatchEvent(new MouseEvent('mouseup', props));
  element.dispatchEvent(new MouseEvent('click', props));
  await element.updateComplete;
}

/**
 * Sends a shift click event to |element|, using PointerEvent.
 */
export async function shiftPointerClick(element: CrLitElement): Promise<void> {
  const xy = middleOfNode(element);
  const props = {
    bubbles: true,
    cancelable: true,
    clientX: xy.x,
    clientY: xy.y,
    buttons: 1,
    shiftKey: true,
  };
  element.dispatchEvent(new PointerEvent('pointerdown', props));
  element.dispatchEvent(new PointerEvent('pointerup', props));
  element.dispatchEvent(new PointerEvent('click', props));
  await element.updateComplete;
}

export function disableLinkClicks() {
  document.addEventListener('click', function(e) {
    if (e.defaultPrevented) {
      return;
    }

    const eventPath = e.composedPath();
    let anchor = null;
    if (eventPath) {
      for (let i = 0; i < eventPath.length; i++) {
        const element = eventPath[i] as HTMLElement;
        if (element.tagName === 'A' && (element as HTMLAnchorElement).href) {
          anchor = element;
          break;
        }
      }
    }

    if (!anchor) {
      return;
    }

    e.preventDefault();
  });
}

export function createSession(
    name: string, windows: ForeignSessionWindow[]): ForeignSession {
  return {
    collapsed: false,
    name,
    modifiedTime: '2 seconds ago',
    tag: name,
    timestamp: 0,
    windows,
  };
}

export function createWindow(tabUrls: string[]): ForeignSessionWindow {
  const tabs: ForeignSessionTab[] = tabUrls.map(function(tabUrl) {
    return {
      direction: '',
      remoteIconUrlForUma: '',
      sessionId: 456,
      timestamp: 0,
      title: tabUrl,
      type: 'tab',
      url: tabUrl,
      windowId: 0,
    };
  });

  return {tabs: tabs, sessionId: 123, timestamp: 0};
}

export function navigateTo(route: string, _app: HistoryAppElement) {
  window.history.replaceState({}, '', route);
  window.dispatchEvent(new CustomEvent('popstate'));
  flush();
}
