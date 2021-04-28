// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './battery_status_card.js';
import './cpu_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './memory_card.js';
import './network_list.js';
import './overview_card.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DiagnosticsBrowserProxy, DiagnosticsBrowserProxyImpl} from './diagnostics_browser_proxy.js';
import {SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'diagnostics-app' is the main page for viewing telemetric system information
 * and running diagnostic tests.
 */
Polymer({
  is: 'diagnostics-app',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * @private {?DiagnosticsBrowserProxy}
   */
  browserProxy_: null,

  properties: {
    /** @private {boolean} */
    showBatteryStatusCard_: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    systemInfoReceived_: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    toastText_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isLoggedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isLoggedIn'),
    },

    /** @type {string} */
    bannerMessage: {
      type: String,
      value: '',
    },

    /** @private {string} */
    scrollingClass_: {
      type: String,
      value: '',
    },

    /** @private {number} */
    scrollTimerId_: {
      type: Number,
      value: -1,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchSystemInfo_();
    this.browserProxy_ = DiagnosticsBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
    this.addCautionBannerEventListeners_();
  },

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then((result) => {
      this.onSystemInfoReceived_(result.systemInfo);
    });
    setTimeout(() => this.recordLateSystemInfo_(), 3000);
  },

  /**
   * @param {!SystemInfo} systemInfo
   * @private
   */
  onSystemInfoReceived_(systemInfo) {
    this.systemInfoReceived_ = true;
    this.showBatteryStatusCard_ = systemInfo.deviceCapabilities.hasBattery;
  },

  /**
   * @private
   */
  recordLateSystemInfo_() {
    if (!this.systemInfoReceived_) {
      console.warn('system info not received within three seconds.');
    }
  },

  /** @protected */
  onSessionLogClick_() {
    this.browserProxy_.saveSessionLog().then(
        /* @type {boolean} */ (success) => {
          const result = success ? 'Success' : 'Failure';
          this.toastText_ =
              loadTimeData.getString(`sessionLogToastText${result}`);
          this.$.toast.show();
        }).catch(() => {/* File selection cancelled */});
  },

  /**
   * @protected
   * @return {boolean}
   */
  isNetworkingEnabled_() {
    return loadTimeData.getBoolean('isNetworkingEnabled');
  },

  /** @private */
  addCautionBannerEventListeners_() {
    window.addEventListener('show-caution-banner', (e) => {
      assert(e.detail.message);
      this.bannerMessage = e.detail.message;
    });

    window.addEventListener('dismiss-caution-banner', () => {
      this.bannerMessage = '';
    });

    window.addEventListener('scroll', () => {
      if (!this.bannerMessage) {
        return;
      }

      // Reset timer since we've received another 'scroll' event.
      if (this.scrollTimerId_ !== -1) {
        this.scrollingClass_ = 'elevation-2';
        clearTimeout(this.scrollTimerId_);
      }

      // Remove box shadow from banner since the user has stopped scrolling
      // for at least 300ms.
      this.scrollTimerId_ =
          window.setTimeout(() => this.scrollingClass_ = '', 300);
    });
  },
});