// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import './d3.min.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'realtime-cpu-chart' is a moving line graph component used to display a
 * realtime cpu usage information.
 */
Polymer({
  is: 'realtime-cpu-chart',

  _template: html`{__html_template__}`,

  /**
   * Helper function to map range of y coordinates to graph height.
   * @private {?d3.LinearScale}
   */
  yAxisScaleFn_: null,

  properties: {
    user: {
      type: Number,
      value: 0,
    },

    system: {
      type: Number,
      value: 0,
    },

    /** @private */
    numDataPoints_: {
      type: Number,
      value: 50,
    },

    /** @private */
    width_: {
      type: Number,
      value: 350,
    },

    /** @private */
    height_: {
      type: Number,
      value: 100,
    },

    /** @private */
    margin_: {
      type: Object,
      value: {top: 10, right: 20, bottom: 10, left: 30},
    },

    /** @private */
    graphWidth_: {
      readOnly: true,
      type: Number,
      computed: 'getGraphDimension_(width_, margin_.left, margin_.right)'
    },

    /** @private */
    graphHeight_: {
      readOnly: true,
      type: Number,
      computed: 'getGraphDimension_(height_, margin_.top, margin_.bottom)'
    }
  },

  /** @override */
  ready() {
    this.setScaling_();
    this.initializeChart_();
  },

  /**
   * Get actual graph dimensions after accounting for margins.
   * @param {number} base value of dimension.
   * @param {...number} margins related to base dimension.
   * @return {number}
   * @private
   */
  getGraphDimension_(base, ...margins) {
    return margins.reduce(((acc, margin) => acc - margin), base);
  },

  /**
   * Sets scaling functions that convert data -> svg coordinates.
   * @private
   */
  setScaling_() {
    // Map y-values (0, 100) to (graphHeight, 0) inverse linearly.
    // Data value of 0 will map to graphHeight, 100 maps to 0.
    this.yAxisScaleFn_ =
        d3.scaleLinear().domain([0, 100]).range([this.graphHeight_, 0]);
  },

  /** @private */
  initializeChart_() {
    const chartGroup = d3.select(this.$$('#chartGroup'));

    // Position chartGroup inside the margin.
    chartGroup.attr(
        'transform',
        'translate(' + this.margin_.left + ',' + this.margin_.top + ')');

    // Draw the y-axis legend and also draw the horizontal gridlines by
    // reversing the ticks back into the chart body.
    chartGroup.select('#gridLines')
        .call(
            d3.axisLeft(/** @type {!d3.LinearScale} */(this.yAxisScaleFn_))
                .ticks(3)                     // Number of y-axis ticks
                .tickSize(-this.graphWidth_)  // Extend the ticks into the
                                              // entire graph as gridlines.
        );
  },
});
