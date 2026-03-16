// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

const TestListenerElementBase = WebUiListenerMixinLit(CrLitElement);

export class TestListenerElement extends TestListenerElementBase {
  static get is() {
    return 'test-listener';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-listener': TestListenerElement;
  }
}

export function setupTestListenerElement(): void {
  customElements.define(TestListenerElement.is, TestListenerElement);
}
