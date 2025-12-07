// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is derived from
// `wpt/service-workers/service-worker/resources/test-helpers.sub.js`
// and thus remains in the ServiceWorker WPT coding style.

// Return true if |state_a| is more advanced than |state_b|.
function is_state_advanced(state_a, state_b) {
  if (state_b === 'installing') {
    switch (state_a) {
      case 'installed':
      case 'activating':
      case 'activated':
      case 'redundant':
        return true;
    }
  }

  if (state_b === 'installed') {
    switch (state_a) {
      case 'activating':
      case 'activated':
      case 'redundant':
        return true;
    }
  }

  if (state_b === 'activating') {
    switch (state_a) {
      case 'activated':
      case 'redundant':
        return true;
    }
  }

  if (state_b === 'activated') {
    switch (state_a) {
      case 'redundant':
        return true;
    }
  }
  return false;
}

function wait_for_state(worker, state) {
  if (!worker || worker.state == undefined) {
    return Promise.reject(new Error(
      'wait_for_state needs a ServiceWorker object to be passed.'));
  }
  if (worker.state === state) {
    return Promise.resolve(state);
  }

  if (is_state_advanced(worker.state, state)) {
    return Promise.reject(new Error(
      `Waiting for ${state} but the worker is already ${worker.state}.`));
  }
  return new Promise(function(resolve, reject) {
      worker.addEventListener('statechange', function() {
          if (worker.state === state) {
            resolve(state);
          }

          if (is_state_advanced(worker.state, state)) {
            reject(new Error(
              `The state of the worker becomes ${worker.state} while waiting` +
                `for ${state}.`));
          }
        });
    });
}
