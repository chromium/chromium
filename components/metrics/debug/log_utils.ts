// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {LogEvent} from './browser_proxy.js';

/**
 * Helper function to convert undefined UMA log types to "Unknown" string.
 * @param type The UMA log's type (i.e., ongoing, independent, or stability).
 * @returns The UMA log's type. "Unknown" if type is undefined.
 */
export function umaLogTypeToString(type: string|undefined) {
  if (!type) {
    return 'Unknown';
  }
  return type;
}

/**
 * Converts a given Unix timestamp into a human-readable string.
 * @param timestampSeconds The timestamp string (seconds since Epoch).
 * @return A human-readable representation of the timestamp (e.g "01/01/1970,
 *     12:00:00 AM").
 */
export function timestampToString(timestampSeconds: string) {
  if (!timestampSeconds.length) {
    // This case should not normally happen, but can happen when the table is
    // empty (a dummy log |EMPTY_LOG| is added, which has an empty timestamp).
    return 'N/A';
  }

  const timestampInt = parseInt(timestampSeconds);
  assert(!isNaN(timestampInt));
  // Multiply by 1000 since the constructor expects milliseconds, but the
  // timestamps are in seconds.
  return new Date(timestampInt * 1000).toLocaleString();
}

/**
 * Converts the size of a log to a human-readable string.
 * @param size The size of the log in bytes.
 * @returns The size of the log in KiB as a string.
 */
export function sizeToString(size: number) {
  if (size < 0) {
    // This case should not normally happen, but can happen when the table is
    // empty (a dummy log |EMPTY_LOG| is added, which has size -1).
    return 'N/A';
  }
  return `${(size / 1024).toFixed(2)} KiB`;
}

/**
 * Converts a log event to a human-readable string.
 * @param event The log event.
 * @returns A human-readable string of the log event.
 */
export function logEventToString(event: LogEvent) {
  let result = `[${new Date(event.timestampMs).toISOString()}] ${event.event}`;
  if (event.message) {
    result += ` (${event.message})`;
  }
  return result;
}

/**
 * Gets the string to display when the events div of a log are collapsed.
 * @param events The list of events of the log.
 * @returns A human-readable string of the last event that occurred.
 */
export function getEventsPeekString(events: LogEvent[]) {
  if (!events.length) {
    return 'N/A';
  }
  // Need to assert that last element exists, otherwise the call to
  // logEventToString() fails to compile.
  const lastEvent = events[events.length - 1];
  assert(lastEvent);
  return logEventToString(lastEvent);
}
