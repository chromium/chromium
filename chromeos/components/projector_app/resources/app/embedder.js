// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ProjectorBrowserProxyImpl} from '../communication/projector_browser_proxy.js';

import {AppTrustedCommFactory, UntrustedAppClient} from './trusted/trusted_app_comm_factory.js';

/**
 * Gets the query string from the URL.
 * For example, if the URL is chrome://projector/annotator/abc, then query
 * is "abc".
 */
function getQuery() {
  if (!document.location.pathname) {
    return '';
  }
  const paths = document.location.pathname.split('/');
  if (paths.length < 1) {
    return '';
  }
  return paths[paths.length - 1];
}

Polymer({
  is: 'app-embedder',

  behaviors: [WebUIListenerBehavior],

  /** @override */
  ready() {
    document.body.querySelector('iframe').src =
        'chrome-untrusted://projector/' + getQuery();

    let client = AppTrustedCommFactory.getPostMessageAPIClient();

    this.addWebUIListener('onNewScreencastPreconditionChanged', (canStart) => {
      if (typeof canStart !== "boolean") {
        console.error(
            'Invalid argument to onNewScreencastPreconditionChanged', canStart);
        return;
      }
      client.onNewScreencastPreconditionChanged(canStart);
    });
  },
});
