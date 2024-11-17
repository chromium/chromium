// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/js/jstemplate_compiled.js';

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface SourceInfo {}
interface Site {}

function initialize() {
  getRequiredElement('submit-update')
      .addEventListener('click', function(event) {
        event.preventDefault();
        chrome.send(
            'update', [{
              popular: {
                overrideURL:
                    getRequiredElement<HTMLInputElement>('override-url').value,
                overrideDirectory:
                    getRequiredElement<HTMLInputElement>('override-directory')
                        .value,
                overrideCountry:
                    getRequiredElement<HTMLInputElement>('override-country')
                        .value,
                overrideVersion:
                    getRequiredElement<HTMLInputElement>('override-version')
                        .value,
              },
            }]);
      });

  getRequiredElement('popular-view-json')
      .addEventListener('click', function(event) {
        event.preventDefault();
        if (getRequiredElement('popular-json-value').textContent === '') {
          chrome.send('viewPopularSitesJson');
        } else {
          getRequiredElement('popular-json-value').textContent = '';
        }
      });

  addWebUiListener('receive-source-info', (state: SourceInfo) => {
    jstProcess(new JsEvalContext(state), getRequiredElement('sources'));
  });
  addWebUiListener('receive-sites', (sites: Site[]) => {
    jstProcess(new JsEvalContext(sites), getRequiredElement('sites'));
  });
  chrome.send('registerForEvents');
}

document.addEventListener('DOMContentLoaded', initialize);
