// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TestCustomHelpBubbleElement} from './test_custom_help_bubble.js';

export function getHtml(this: TestCustomHelpBubbleElement) {
  // clang-format off
  return html`
<button id="cancel" @click="${this.onCancelButton_}">Cancel</button>
<button id="dismiss" @click="${this.onDismissButton_}">Dismiss</button>
<button id="snooze" @click="${this.onSnoozeButton_}">Snooze</button>
<button id="action" @click="${this.onActionButton_}">Action</button>
`;
  // clang-format on
}
