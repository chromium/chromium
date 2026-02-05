// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40108835): Consider making this a custom HTML element.
// The font size slider is visible on desktop platforms.
class FontSizeSlider {
  static #instance = null;

  static getInstance() {
    return FontSizeSlider.#instance ??
        (FontSizeSlider.#instance = new FontSizeSlider());
  }

  constructor() {
    this.element = $('font-size-selection');
    this.baseSize = 16;
    // These scales are applied to a base size of 16px.
    this.fontSizeScale = [0.875, 0.9375, 1, 1.125, 1.25, 1.5, 1.75, 2, 2.5, 3];

    this.element.addEventListener('input', (e) => {
      const scale = this.fontSizeScale[e.target.value];
      this.useFontScaling(scale);
      distiller.storeFontScalingPref(parseFloat(scale));
    });

    this.tickmarks = document.createElement('datalist');
    this.tickmarks.setAttribute('class', 'tickmarks');
    this.element.after(this.tickmarks);

    for (let i = 0; i < this.fontSizeScale.length; i++) {
      const option = document.createElement('option');
      option.setAttribute('value', i);
      option.textContent = this.fontSizeScale[i] * this.baseSize;
      this.tickmarks.appendChild(option);
    }
    this.element.value = 2;
    this.update(this.element.value);
  }

  // TODO(meredithl): validate |scale| and snap to nearest supported font size.
  useFontScaling(scale, restoreCenter = true) {
    this.element.value = this.fontSizeScale.indexOf(scale);
    document.documentElement.style.fontSize = scale * this.baseSize + 'px';
    this.update(this.element.value);
  }

  update(position) {
    this.element.style.setProperty(
        '--fontSizePercent',
        (position / (this.fontSizeScale.length - 1) * 100) + '%');
    this.element.setAttribute(
        'aria-valuetext', this.fontSizeScale[position] + 'px');
    for (let option = this.tickmarks.firstChild; option != null;
         option = option.nextSibling) {
      const isBeforeThumb = option.value < position;
      option.classList.toggle('before-thumb', isBeforeThumb);
      option.classList.toggle('after-thumb', !isBeforeThumb);
    }
  }

  useBaseFontSize(size) {
    this.baseSize = size;
    this.update(this.element.value);
  }
}
