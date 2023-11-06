// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';
import './about.js';
import './data.js';
import './sync_node_browser.js';
import './user_events.js';
import './traffic_log.js';
import './search.js';
import './strings.m.js';
import './invalidations.js';

import {assert} from 'chrome://resources/js/assert.js';
// <if expr="chromeos_ash">
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';
// </if>

// Allow platform specific CSS rules.
//
// TODO(akalin): BMM and options page does something similar, too.
// Move this to util.js.
// <if expr="is_win">
document.documentElement.setAttribute('os', 'win');
// </if>

const tabBox = document.querySelector('cr-tab-box');
assert(tabBox);
tabBox.hidden = false;

// <if expr="chromeos_ash">
// Updates the os-link-container so that it lets the user open
// os://sync-internals window if Lacros is enabled.
function updateOsLink() {
  sendWithPromise('isLacrosEnabled').then(function(isLacrosEnabled: boolean) {
    const osLinkContainer = $('os-link-container');
    if (osLinkContainer) {
      osLinkContainer.hidden = !isLacrosEnabled;
    }
  });

  const osLinkHref = $('os-link-href');
  if (osLinkHref) {
    const handleClick = function(event: MouseEvent) {
      event.preventDefault();

      // Note: make sure this name matches the C++ constant
      // `kOpenLacrosSyncInternals`.
      chrome.send('openLacrosSyncInternals');
    };

    osLinkHref.onclick = handleClick as EventListener;
    osLinkHref.onauxclick = function(event: MouseEvent) {
      // Make middle-clicks have the same effects as Ctrl+clicks
      if (event.button === 1) {
        handleClick(event);
      }
    } as EventListener;
  }
}
updateOsLink();
// </if>
