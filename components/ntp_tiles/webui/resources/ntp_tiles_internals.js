// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

const initialize = function() {
  $('submit-update').addEventListener('click', function(event) {
    event.preventDefault();
    chrome.send('update', [{
                  popular: {
                    overrideURL: $('override-url').value,
                    overrideDirectory: $('override-directory').value,
                    overrideCountry: $('override-country').value,
                    overrideVersion: $('override-version').value,
                  },
                }]);
  });

  $('popular-view-json').addEventListener('click', function(event) {
    event.preventDefault();
    if ($('popular-json-value').textContent === '') {
      chrome.send('viewPopularSitesJson');
    } else {
      $('popular-json-value').textContent = '';
    }
  });

  addWebUiListener('receive-source-info', state => {
    jstProcess(new JsEvalContext(state), $('sources'));
  });
  addWebUiListener('receive-sites', sites => {
    jstProcess(new JsEvalContext(sites), $('sites'));
  });
  chrome.send('registerForEvents');
};

document.addEventListener('DOMContentLoaded', initialize);
