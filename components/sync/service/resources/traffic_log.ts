// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';

let instance: TrafficLogTag|null = null;

export interface ProtocolEvent {
  time: string;
}

class TrafficLogTag {
  container: HTMLElement|null = null;
  protocolEvents: ProtocolEvent[] = [];
  knownEventTimestamps: Set<string> = new Set();

  /**
   * Helper to determine if the window is scrolled to its bottom limit.
   * @return true if the container is scrolled to the bottom
   */
  private isScrolledToBottom_(): boolean {
    return (window.innerHeight + window.scrollY) >= document.body.offsetHeight;
  }

  /**
   * Helper to scroll the window to its bottom.
   */
  private scrollToBottom_() {
    window.scrollTo(0, document.body.scrollHeight);
  }

  /**
   * Callback for incoming protocol events.
   * @param details The protocol event.
   */
  private onReceivedProtocolEvent_(details: ProtocolEvent) {
    if (this.knownEventTimestamps.has(details.time)) {
      return;
    }

    this.knownEventTimestamps.add(details.time);
    this.protocolEvents.push(details);

    const shouldScrollDown = this.isScrolledToBottom_();

    assert(this.container);
    jstProcess(
        new JsEvalContext({events: this.protocolEvents}), this.container);

    if (shouldScrollDown) {
      this.scrollToBottom_();
    }
  }

  /**
   * Toggles the given traffic event entry div's "expanded" state.
   * @param e the click event that triggered the toggle.
   */
  private expandListener_(e: Event) {
    if ((e.target as HTMLElement).classList.contains('proto')) {
      // We ignore proto clicks to keep it copyable.
      return;
    }
    let trafficEventDiv = e.target as HTMLElement;
    // Click might be on div's child.
    if (trafficEventDiv.nodeName !== 'DIV' && trafficEventDiv.parentNode) {
      trafficEventDiv = trafficEventDiv.parentNode as HTMLElement;
    }
    trafficEventDiv.classList.toggle('traffic-event-entry-expanded-fullscreen');
  }

  /**
   * Attaches a listener to the given traffic event entry div.
   */
  addExpandListener(element: HTMLElement) {
    element.addEventListener('click', this.expandListener_, false);
  }

  onLoad() {
    const container = document.querySelector<HTMLElement>(
        '#traffic-event-fullscreen-container');
    assert(container);
    this.container = container;

    addWebUiListener(
        'onProtocolEvent', this.onReceivedProtocolEvent_.bind(this));

    // Make the prototype jscontent element disappear.
    jstProcess(new JsEvalContext({}), this.container);
  }

  static getInstance(): TrafficLogTag {
    return instance || (instance = new TrafficLogTag());
  }
}

// For JS eval.
Object.assign(window, {TrafficLogTag});

document.addEventListener('DOMContentLoaded', () => {
  TrafficLogTag.getInstance().onLoad();
});
