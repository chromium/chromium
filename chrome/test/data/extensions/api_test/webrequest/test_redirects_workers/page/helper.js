// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is derived from
// `wpt/service-workers/service-worker/resources/test-helpers.sub.js`
// and thus remains in the ServiceWorker WPT coding style.

// Return true if |stateA| is more advanced than |stateB|.
function isStateAdvanced(stateA, stateB) {
  // Note: "No default" is used for eslint'ing. These don't have defaults
  // because they intentionally fallthrough to the false case at the end of
  // the function.

  if (stateB === 'installing') {
    switch (stateA) {
      case 'installed':
      case 'activating':
      case 'activated':
      case 'redundant':
        return true;
        // No default
    }
  }

  if (stateB === 'installed') {
    switch (stateA) {
      case 'activating':
      case 'activated':
      case 'redundant':
        return true;
        // No default
    }
  }

  if (stateB === 'activating') {
    switch (stateA) {
      case 'activated':
      case 'redundant':
        return true;
        // No default
    }
  }

  if (stateB === 'activated') {
    switch (stateA) {
      case 'redundant':
        return true;
        // No default
    }
  }
  return false;
}

function waitForState(worker, state) {
  if (!worker || worker.state === undefined) {
    return Promise.reject(
        new Error('waitForState needs a ServiceWorker object to be passed.'));
  }
  if (worker.state === state) {
    return Promise.resolve(state);
  }

  if (isStateAdvanced(worker.state, state)) {
    return Promise.reject(new Error(
        `Waiting for ${state} but the worker is already ${worker.state}.`));
  }
  return new Promise(function(resolve, reject) {
    worker.addEventListener('statechange', function() {
      if (worker.state === state) {
        resolve(state);
      }

      if (isStateAdvanced(worker.state, state)) {
        reject(new Error(
            `The state of the worker becomes ${worker.state} while waiting` +
            `for ${state}.`));
      }
    });
  });
}
