// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CalendarEvent} from 'chrome://new-tab-page/google_calendar.mojom-webui.js';

export function createEvents(num: number): CalendarEvent[] {
  const events: CalendarEvent[] = [];
  for (let i = 0; i < num; i++) {
    events.push({title: `Test Event ${i}`});
  }
  return events;
}
