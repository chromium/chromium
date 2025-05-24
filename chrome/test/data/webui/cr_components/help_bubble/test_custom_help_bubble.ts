// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CustomHelpBubbleHandlerInterface} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import {CustomHelpBubbleUserAction} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble.mojom-webui.js';
import {CustomHelpBubbleProxyImpl} from 'chrome://resources/cr_components/help_bubble/custom_help_bubble_proxy.js';
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

  private handler_: CustomHelpBubbleHandlerInterface;

  constructor() {
    super();
    this.handler_ = CustomHelpBubbleProxyImpl.getInstance().getHandler();
  }

  protected onCancelButton_() {
    this.handler_.notifyUserAction(CustomHelpBubbleUserAction.kCancel);
  }

  protected onDismissButton_() {
    this.handler_.notifyUserAction(CustomHelpBubbleUserAction.kDismiss);
  }

  protected onSnoozeButton_() {
    this.handler_.notifyUserAction(CustomHelpBubbleUserAction.kSnooze);
  }

  protected onActionButton_() {
    this.handler_.notifyUserAction(CustomHelpBubbleUserAction.kAction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-custom-help-bubble': TestCustomHelpBubbleElement;
  }
}

customElements.define(
    TestCustomHelpBubbleElement.is, TestCustomHelpBubbleElement);
