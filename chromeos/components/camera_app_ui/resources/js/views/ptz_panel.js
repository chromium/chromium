// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AsyncJobQueue} from '../async_job_queue.js';
import * as dom from '../dom.js';
import * as nav from '../nav.js';
import * as state from '../state.js';
import {ViewName} from '../type.js';

// eslint-disable-next-line no-unused-vars
import {View} from './view.js';

/**
 * View controller for PTZ panel.
 */
export class PTZPanel extends View {
  /**
   * @public
   */
  constructor() {
    super(
        ViewName.PTZ_PANEL, /* dismissByEsc */ true,
        /* dismissByBkgndClick */ true);

    /**
     * Video track of opened stream having PTZ support.
     * @private {?MediaStreamTrack}
     */
    this.track_ = null;

    /**
     * @private {!HTMLDivElement}
     * @const
     */
    this.panel_ = dom.get('#panel-container', HTMLDivElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.panLeft_ = dom.get('#pan-left', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.panRight_ = dom.get('#pan-right', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.tiltUp_ = dom.get('#tilt-up', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.tiltDown_ = dom.get('#tilt-down', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.zoomIn_ = dom.get('#zoom-in', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.zoomOut_ = dom.get('#zoom-out', HTMLButtonElement);

    state.addObserver(state.State.STREAMING, (streaming) => {
      if (!streaming && state.get(this.name)) {
        nav.close(this.name);
      }
    });
  }

  /**
   * Binds buttons with the attribute name to be controlled.
   * @param {string} attr One of pan, tilt, zoom attribute name to be bound.
   * @param {!HTMLButtonElement} incBtn Button for increasing the value.
   * @param {!HTMLButtonElement} decBtn Button for decreasing the value.
   * @param {!MediaSettingsRange} range Available value range.
   */
  bind_(attr, incBtn, decBtn, range) {
    const {min, max, step} = range;
    const getCurrent = () => this.track_.getSettings()[attr];
    const checkDisabled = () => {
      const current = getCurrent();
      decBtn.disabled = current - step < min;
      incBtn.disabled = current + step > max;
    };
    checkDisabled();

    const queue = new AsyncJobQueue();
    const onClick = (delta) => {
      return () => {
        queue.push(async () => {
          if (this.track_.readyState !== 'live') {
            return;
          }
          // TODO(b/172881094): Normalize steps to at most 10.
          const next = getCurrent() + delta;
          if (next < min || next > max) {
            return;
          }
          await this.track_.applyConstraints({advanced: [{[attr]: next}]});
          checkDisabled();
        });
      };
    };

    // TODO(b/183661327): Polish holding button behavior.
    incBtn.onclick = onClick(step);
    decBtn.onclick = onClick(-step);
  }

  /**
   * @override
   */
  entering(stream) {
    const {bottom, right} =
        dom.get('#open-ptz-panel', HTMLButtonElement).getBoundingClientRect();
    this.panel_.style.bottom = `${window.innerHeight - bottom}px`;
    this.panel_.style.left = `${right + 6}px`;

    this.track_ = stream.getVideoTracks()[0];
    const {pan, tilt, zoom} = this.track_.getCapabilities();

    state.set(state.State.HAS_PAN_SUPPORT, pan !== undefined);
    state.set(state.State.HAS_TILT_SUPPORT, tilt !== undefined);
    state.set(state.State.HAS_ZOOM_SUPPORT, zoom !== undefined);

    if (pan !== undefined) {
      this.bind_('pan', this.panRight_, this.panLeft_, pan);
    }

    if (tilt !== undefined) {
      this.bind_('tilt', this.tiltUp_, this.tiltDown_, tilt);
    }

    if (zoom !== undefined) {
      this.bind_('zoom', this.zoomIn_, this.zoomOut_, zoom);
    }
  }
}
