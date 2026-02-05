// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The zooming speed relative to pinching speed.
const FONT_SCALE_MULTIPLIER = 0.5;

const MIN_SPAN_LENGTH = 20;

// Only defined on Android.
class Pincher {
  // When users pinch in Reader Mode, the page would zoom in or out as if it
  // is a normal web page allowing user-zoom. At the end of pinch gesture, the
  // page would do text reflow. These pinch-to-zoom and text reflow effects
  // are not native, but are emulated using CSS and JavaScript.
  //
  // In order to achieve near-native zooming and panning frame rate, fake 3D
  // transform is used so that the layer doesn't repaint for each frame.
  //
  // After the text reflow, the web content shown in the viewport should
  // roughly be the same paragraph before zooming.
  //
  // The control point of font size is the html element, so that both "em" and
  // "rem" are adjusted.
  //
  // TODO(wychen): Improve scroll position when elementFromPoint is body.

  static #instance = null;

  static getInstance() {
    return Pincher.#instance ?? (Pincher.#instance = new Pincher());
  }

  constructor() {
    // This has to be in sync with largest 'font-size' in distilledpage_{}.css.
    // This value is hard-coded because JS might be injected before CSS is
    // ready. See crbug.com/1004663.
    this.baseSize = 16;
    this.pinching = false;
    this.fontSizeAnchor = 1.0;

    this.focusElement = null;
    this.focusPos = 0;
    this.initClientMid = null;

    this.clampedScale = 1.0;

    this.lastSpan = null;
    this.lastClientMid = null;

    this.scale = 1.0;
    this.shiftX = 0;
    this.shiftY = 0;

    window.addEventListener('touchstart', (e) => {
      this.handleTouchStart(e);
    }, {passive: false});
    window.addEventListener('touchmove', (e) => {
      this.handleTouchMove(e);
    }, {passive: false});
    window.addEventListener('touchend', (e) => {
      this.handleTouchEnd(e);
    }, {passive: false});
    window.addEventListener('touchcancel', (e) => {
      this.handleTouchCancel(e);
    }, {passive: false});
  }

  clampScaleFun(v) {
    // The line below is dynamically modified from C++, and the result is
    // checked DomDistillerViewerTest by RegExp, so do not reformat!
    return /* PINCH_SCALE */ Math.max($MIN_SCALE, Math.min($MAX_SCALE, v));
  }

  /** @private */
  refreshTransform_() {
    const slowedScale = Math.exp(Math.log(this.scale) * FONT_SCALE_MULTIPLIER);
    this.clampedScale = this.clampScaleFun(this.fontSizeAnchor * slowedScale);

    // Use "fake" 3D transform so that the layer is not repainted.
    // With 2D transform, the frame rate would be much lower.
    // clang-format off
    document.body.style.transform =
        'translate3d(' + this.shiftX + 'px,'
                       + this.shiftY + 'px, 0px)' +
        'scale(' + this.clampedScale / this.fontSizeAnchor + ')';
    // clang-format on
  }

  /** @private */
  saveCenter_(clientMid) {
    // Try to preserve the pinching center after text reflow.
    // This is accurate to the HTML element level.
    this.focusElement = document.elementFromPoint(clientMid.x, clientMid.y);
    const rect = this.focusElement.getBoundingClientRect();
    this.initClientMid = clientMid;
    this.focusPos =
        (this.initClientMid.y - rect.top) / (rect.bottom - rect.top);
  }

  /** @private */
  restoreCenter_() {
    const rect = this.focusElement.getBoundingClientRect();
    const targetTop = this.focusPos * (rect.bottom - rect.top) + rect.top +
        document.scrollingElement.scrollTop -
        (this.initClientMid.y + this.shiftY);
    document.scrollingElement.scrollTop = targetTop;
  }

  /** @private */
  endPinch_() {
    this.pinching = false;

    document.body.style.transformOrigin = '';
    document.body.style.transform = '';
    document.documentElement.style.fontSize =
        this.clampedScale * this.baseSize + 'px';

    this.restoreCenter_();

    let img = $('fontscaling-img');
    if (!img) {
      img = document.createElement('img');
      img.id = 'fontscaling-img';
      img.style.display = 'none';
      document.body.appendChild(img);
    }
    img.src = '/savefontscaling/' + this.clampedScale;
  }

  /** @private */
  touchSpan_(e) {
    const count = e.touches.length;
    const mid = this.touchClientMid_(e);
    let sum = 0;
    for (let i = 0; i < count; i++) {
      const dx = (e.touches[i].clientX - mid.x);
      const dy = (e.touches[i].clientY - mid.y);
      sum += Math.hypot(dx, dy);
    }
    // Avoid very small span.
    return Math.max(MIN_SPAN_LENGTH, sum / count);
  }

  /** @private */
  touchClientMid_(e) {
    const count = e.touches.length;
    let sumX = 0;
    let sumY = 0;
    for (let i = 0; i < count; i++) {
      sumX += e.touches[i].clientX;
      sumY += e.touches[i].clientY;
    }
    return {x: sumX / count, y: sumY / count};
  }

  /** @private */
  touchPageMid_(e) {
    const clientMid = this.touchClientMid_(e);
    return {
      x: clientMid.x - e.touches[0].clientX + e.touches[0].pageX,
      y: clientMid.y - e.touches[0].clientY + e.touches[0].pageY,
    };
  }

  handleTouchStart(e) {
    if (e.touches.length < 2) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    if (e.touches.length > 2) {
      this.lastSpan = span;
      this.lastClientMid = clientMid;
      this.refreshTransform_();
      return;
    }

    this.scale = 1;
    this.shiftX = 0;
    this.shiftY = 0;

    this.pinching = true;
    this.fontSizeAnchor =
        parseFloat(getComputedStyle(document.documentElement).fontSize) /
        this.baseSize;

    const pinchOrigin = this.touchPageMid_(e);
    document.body.style.transformOrigin =
        pinchOrigin.x + 'px ' + pinchOrigin.y + 'px';

    this.saveCenter_(clientMid);

    this.lastSpan = span;
    this.lastClientMid = clientMid;

    this.refreshTransform_();
  }

  handleTouchMove(e) {
    if (!this.pinching) {
      return;
    }
    if (e.touches.length < 2) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    this.scale *= this.touchSpan_(e) / this.lastSpan;
    this.shiftX += clientMid.x - this.lastClientMid.x;
    this.shiftY += clientMid.y - this.lastClientMid.y;

    this.refreshTransform_();

    this.lastSpan = span;
    this.lastClientMid = clientMid;
  }

  handleTouchEnd(e) {
    if (!this.pinching) {
      return;
    }
    e.preventDefault();

    const span = this.touchSpan_(e);
    const clientMid = this.touchClientMid_(e);

    if (e.touches.length >= 2) {
      this.lastSpan = span;
      this.lastClientMid = clientMid;
      this.refreshTransform_();
      return;
    }

    this.endPinch_();
  }

  handleTouchCancel(e) {
    if (!this.pinching) {
      return;
    }
    this.endPinch_();
  }

  reset() {
    this.scale = 1;
    this.shiftX = 0;
    this.shiftY = 0;
    this.clampedScale = 1;
    document.documentElement.style.fontSize =
        this.clampedScale * this.baseSize + 'px';
  }

  status() {
    return {
      scale: this.scale,
      clampedScale: this.clampedScale,
      shiftX: this.shiftX,
      shiftY: this.shiftY,
    };
  }

  useFontScaling(scaling, restoreCenter = true) {
    if (restoreCenter) {
      this.saveCenter_({x: window.innerWidth / 2, y: window.innerHeight / 2});
    }
    this.shiftX = 0;
    this.shiftY = 0;
    document.documentElement.style.fontSize = scaling * this.baseSize + 'px';
    this.clampedScale = scaling;
    if (restoreCenter) {
      this.restoreCenter_();
    }
  }

  useBaseFontSize(size) {
    this.baseSize = size;
    this.reset();
  }
}
