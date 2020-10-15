// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import './d3.min.js';

import {assert} from 'chrome://resources/js/assert.m.js';
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
   * Helper function to map range of x coordinates to graph width.
   * @private {?d3.LinearScale}
   */
  xAxisScaleFn_: null,

  /**
   * Helper function to map range of y coordinates to graph height.
   * @private {?d3.LinearScale}
   */
  yAxisScaleFn_: null,

  /** @private {!Array<Object>} */
  data_: [],

  /**
   * Unix timestamp in milliseconds of last data update.
   * @private {number}
   */
  dataLastUpdated_: 0,

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
    refreshInterval_: {
      type: Number,
      value: 200,  // in milliseconds.
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
  created() {
    // Initialize the data array with data outside the chart boundary.
    for (var i = 0; i < this.numDataPoints_; ++i) {
      this.data_.push({user: -1, system: -1});
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
    // Map y-values [0, 100] to [graphHeight, 0] inverse linearly.
    // Data value of 0 will map to graphHeight, 100 maps to 0.
    this.yAxisScaleFn_ =
        d3.scaleLinear().domain([0, 100]).range([this.graphHeight_, 0]);

    // Map x-values [0, numDataPoints - 3] to [0, graphWidth] linearly.
    // Data value of 0 maps to 0, and (numDataPoints - 3) maps to graphWidth.
    // numDataPoints is subtracted since 1) data array is zero based, and
    // 2) to smooth out the curve function.
    this.xAxisScaleFn_ =
        d3.scaleLinear().domain([0, this.numDataPoints_ - 3]).range([
          0, this.graphWidth_
        ]);
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
            d3.axisLeft(/** @type {!d3.LinearScale} */ (this.yAxisScaleFn_))
                .ticks(3)                     // Number of y-axis ticks
                .tickSize(-this.graphWidth_)  // Extend the ticks into the
                                              // entire graph as gridlines.
        );

    const plotGroup = d3.select(this.$$('#plotGroup'));

    // Feed data array to the plot group.
    plotGroup.datum(this.data_);

    // Select each line and configure the transition for animation.
    // d3.transition API @ https://github.com/d3/d3-transition#d3-transition.
    // d3.easing API @ https://github.com/d3/d3-ease#api-reference.
    plotGroup.select('.user-line')
        .transition()
        .duration(this.refreshInterval_)
        .ease(d3.easeLinear)  // Linear transition
        .on('start', this.drawChartLine_.bind(this, 'user-line'));
    plotGroup.select('.system-line')
        .transition()
        .duration(this.refreshInterval_)
        .ease(d3.easeLinear)  // Linear transition
        .on('start', this.drawChartLine_.bind(this, 'system-line'));
  },

  /**
   * @param {string} lineClass class string for <path> element.
   * @return {d3.Line}
   * @private
   */
  getLineDefinition_(lineClass) {
    return d3
        .line()
        // Curved line
        .curve(d3.curveBasis)
        // Take the index of each data as x values.
        .x((data, i) => this.xAxisScaleFn_(i))
        // Pick system or user numbers to use as y values.
        .y(data => this.yAxisScaleFn_(
               lineClass === 'user-line' ? data.user : data.system));
  },

  /**
   * Takes a snapshot of current CPU readings and appends to the data array.
   * This method is called by each line after each transition.
   * @private
   */
  appendDataSnapshot_() {
    const now = new Date().getTime();

    // We only want to append the data once per refreshInterval cycle even with
    // multiple lines. Roughly limit the call so that at least half of the
    // refreshInterval has elapsed since the last update.
    if (now - this.dataLastUpdated_ > this.refreshInterval_ / 2) {
      this.dataLastUpdated_ = now;
      this.data_.push({user: this.user, system: this.system});
      this.data_.shift();
    }
  },

  /**
   * @param {string} lineClass class string for <path> element.
   * @private
   */
  drawChartLine_(lineClass) {
    this.appendDataSnapshot_();

    const lineElement = assert(this.$$(`path.${lineClass}`));

    // Reset the animation (transform) and redraw the line with new data.
    // this.data_ is already associated with the plotGroup, so no need to
    // specify it directly here.
    d3.select(lineElement)
        .attr('d', this.getLineDefinition_(lineClass))
        .attr('transform', null);

    // Start animation of the line towards left by one tick outside the chart
    // boundary, then repeat the process.
    d3.active(lineElement)
        .attr('transform', 'translate(' + this.xAxisScaleFn_(-1) + ', 0)')
        .transition()
        .on('start', this.drawChartLine_.bind(this, lineClass));
  },
});
