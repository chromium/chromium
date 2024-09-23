// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Attachment, CalendarEvent} from 'chrome://new-tab-page/calendar_data.mojom-webui.js';
import type {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

// Microseconds between windows and unix epoch.
const kWindowsToUnixEpochOffset: bigint = 11644473600000000n;

export function toTime(time: Date): Time {
  return {
    internalValue: BigInt(time.valueOf()) * 1000n + kWindowsToUnixEpochOffset,
  };
}

export function createAttachments(num: number): Attachment[] {
  const attachments: Attachment[] = [];
  for (let i = 0; i < num; i++) {
    attachments.push({
      title: `Attachment ${i}`,
      iconUrl: {url: `https://foo.com/attachment${i}`},
      resourceUrl: {url: `https://foo.com/attachmet${i}`},
    });
  }
  return attachments;
}

export function createEvent(
    index: number, overrides?: Partial<CalendarEvent>): CalendarEvent {
  return Object.assign(
      {
        title: `Test Event ${index}`,
        startTime: {internalValue: 1230000000000n * BigInt(index)},
        endTime: {internalValue: 1230000000000n * BigInt(index)},
        url: {url: `https://foo.com/${index}`},
        location: `Location ${index}`,
        attachments: createAttachments(3),
        conferenceUrl: {url: `https://foo.com/conference${index}`},
        isAccepted: true,
        hasOtherAttendee: true,
      },
      overrides);
}

export function createEvents(num: number): CalendarEvent[] {
  const events: CalendarEvent[] = [];
  for (let i = 0; i < num; i++) {
    events.push(createEvent(i));
  }
  return events;
}
