// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import { BrowserProxy } from './browser_proxy.js';
import { Status, type State } from './kiosk_vision_internals.mojom-webui.js';
import {
  PolymerElement,
} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import { getTemplate } from './app.html.js';

export interface KioskVisionInternalsAppElement {
  $: {
    'cameraFeed': HTMLVideoElement,
    'overlay': HTMLCanvasElement,
  };
}

export class KioskVisionInternalsAppElement extends PolymerElement {
  static get is() {
    return 'kiosk-vision-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state_: {
        type: Object,
        observer: 'stateChanged_',
      },
    };
  }

  private browserProxy_: BrowserProxy;
  private state_: State;
  private resizeObserver_: ResizeObserver;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxy.getInstance();
    this.browserProxy_.callbackRouter.display.addListener(
      (state: State) => { this.state_ = state; });
    this.state_ = { status: Status.kUnknown, boxes: [] };
    this.resizeObserver_ = new ResizeObserver(this.resizeCallback_());
  }

  override ready() {
    super.ready();
    this.resizeObserver_.observe(this.$.cameraFeed);
  }

  private statusIs_(state: State, status: keyof typeof Status): boolean {
    return state.status === Status[status];
  }

  private async stateChanged_(state: State) {
    if (state.status !== Status.kRunning) {
      this.$.cameraFeed.srcObject = null;
      return;
    }
    this.$.cameraFeed.srcObject ??= await getCameraStream();
    draw(state, this.$.overlay);
  }

  private resizeCallback_(): ResizeObserverCallback {
    return (entries: ResizeObserverEntry[]) => {
      if (entries.length === 0 || entries[0].contentBoxSize.length === 0) {
        return;
      }
      const boxSize = entries[0].contentBoxSize[0];
      this.$.overlay.width = boxSize.inlineSize
      this.$.overlay.height = boxSize.blockSize;
      this.stateChanged_(this.state_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'kiosk-vision-internals-app': KioskVisionInternalsAppElement;
  }
}

customElements.define(
  KioskVisionInternalsAppElement.is, KioskVisionInternalsAppElement);

function draw(state: State, overlay: HTMLCanvasElement) {
  const ctx = overlay.getContext('2d');
  if (ctx == null) {
    return console.error('overlay.getContext is null');
  }

  ctx.clearRect(0, 0, overlay.width, overlay.height);

  // TODO(crbug.com/345457885): Fix coordinates based on `overlay` size.
  for (const { x, y, width, height } of state.boxes) {
    ctx.beginPath();
    ctx.rect(x, y, width, height);
    ctx.lineWidth = 1;
    ctx.strokeStyle = "red";
    ctx.stroke();
  }
}

function getCameraStream(): Promise<MediaStream | null> {
  return navigator.mediaDevices.getUserMedia({
    video: { aspectRatio: 4 / 3 },
    audio: false,
  }).catch((error) => {
    console.error('Failed to get camera stream:', error);
    return null;
  });
}
