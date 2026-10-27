// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import {browserProxyFactory, CustomHelpBubbleUserAction} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './test_custom_help_bubble.html.js';

export interface TestCustomHelpBubbleElement {}

export class TestCustomHelpBubbleElement extends CrLitElement {
  static get is() {
    return 'test-custom-help-bubble';
  }

  override render() {
    return getHtml.bind(this)();
  }

  private browserProxy_: BrowserProxy = browserProxyFactory.getInstance();

  protected onCancelClick_() {
    this.browserProxy_.handler.notifyUserAction(
        CustomHelpBubbleUserAction.kCancel);
  }

  protected onDismissClick_() {
    this.browserProxy_.handler.notifyUserAction(
        CustomHelpBubbleUserAction.kDismiss);
  }

  protected onSnoozeClick_() {
    this.browserProxy_.handler.notifyUserAction(
        CustomHelpBubbleUserAction.kSnooze);
  }

  protected onActionClick_() {
    this.browserProxy_.handler.notifyUserAction(
        CustomHelpBubbleUserAction.kAction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-custom-help-bubble': TestCustomHelpBubbleElement;
  }
}

customElements.define(
    TestCustomHelpBubbleElement.is, TestCustomHelpBubbleElement);
