// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import { BrowserProxy } from './browser_proxy.js';
import {
  Status,
  type State,
  type Box,
  type Face,
} from './kiosk_vision_internals.mojom-webui.js';
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
    this.state_ = { status: Status.kUnknown, boxes: [], faces: [] };
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
      const { inlineSize, blockSize } = entries[0].contentBoxSize[0];
      this.$.overlay.width = inlineSize;
      this.$.overlay.height = blockSize;
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

  for (const box of state.boxes) { drawBox(ctx, overlay, box); }
  for (const face of state.faces) { drawFace(ctx, overlay, face); }
}

const RED = "#f87171";
const GREEN = "#a3e635";

function drawBox(
  ctx: CanvasRenderingContext2D,
  overlay: HTMLCanvasElement,
  box: Box,
  color: string = RED,
) {
  const { x, y } = toCanvasCoordinates(overlay, box.x, box.y);
  const { x: width, y: height } =
    toCanvasCoordinates(overlay, box.width, box.height);
  ctx.beginPath();
  ctx.rect(x, y, width, height);
  ctx.lineWidth = 4;
  ctx.strokeStyle = color;
  ctx.stroke();
  ctx.closePath();
}

function drawFace(
  ctx: CanvasRenderingContext2D,
  overlay: HTMLCanvasElement,
  face: Face,
) {
  // Draw a green box if the person is looking at the camera.
  if (isLookingAtTheCamera(face)) {
    return drawBox(ctx, overlay, face.box, GREEN);
  }

  // Draw a red box and an arrow if the person is not looking at the camera.
  drawBox(ctx, overlay, face.box);
  const { x: centerX, y: centerY } = boxCenter(face.box);
  const { x: x0, y: y0 } = toCanvasCoordinates(overlay, centerX, centerY);
  const { x: x1, y: y1 } =
    toCanvasCoordinates(overlay, centerX + face.pan, centerY - face.tilt);
  const headLength = 10;
  const dx = x1 - x0;
  const dy = y1 - y0;
  const angle = Math.atan2(dy, dx);
  ctx.beginPath();
  ctx.strokeStyle = RED;
  ctx.lineWidth = 4;
  // Draw the arrow body.
  ctx.moveTo(x0, y0);
  ctx.lineTo(x1, y1);
  // Draw the arrow head.
  ctx.lineTo(
    x1 - headLength * Math.cos(angle - Math.PI / 6),
    y1 - headLength * Math.sin(angle - Math.PI / 6)
  );
  ctx.moveTo(x1, y1);
  ctx.lineTo(
    x1 - headLength * Math.cos(angle + Math.PI / 6),
    y1 - headLength * Math.sin(angle + Math.PI / 6)
  );
  ctx.stroke();
  ctx.closePath();
}

function boxCenter({ x, y, width, height }: Box): { x: number, y: number } {
  return { x: x + width / 2, y: y + height / 2 };
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

// Chrome emits Box dimensions on a 569x320 grid. This maps dimensions to
// `canvas.width` x `canvas.height` sizes.
function toCanvasCoordinates(
  canvas: HTMLCanvasElement,
  x: number,
  y: number,
): { x: number, y: number } {
  const FRAME_WIDTH = 569;
  const FRAME_HEIGHT = 320;
  return {
    x: scaleCoordinate(x, FRAME_WIDTH, canvas.width),
    y: scaleCoordinate(y, FRAME_HEIGHT, canvas.height),
  }
}

function scaleCoordinate(x: number, currentMax: number, newMax: number) {
  return x / currentMax * newMax;
}

function isLookingAtTheCamera(face: Face): boolean {
  const HORIZONTAL_THRESHOLD = 10;
  const VERTICAL_THRESHOLD = 6;
  return Math.abs(face.pan) < HORIZONTAL_THRESHOLD
    && Math.abs(face.tilt) < VERTICAL_THRESHOLD;
}
