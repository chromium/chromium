// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Repeatedly runs a query selector until it finds an element.
 *
 * @param {string} query
 * @return {Promise<Element>}
 */
function waitForNode(query) {
  const node = document.body;
  const existingElement = node.querySelector(query);
  if (existingElement) {
    return Promise.resolve(existingElement);
  }
  console.log('Waiting for ' + query);
  return new Promise(resolve => {
    const observer = new MutationObserver((mutationList, observer) => {
      const element = node.querySelector(query);
      if (element) {
        resolve(element);
        observer.disconnect();
      }
    });
    observer.observe(node, {childList: true, subtree: true});
  });
}

/**
 * Acts on received TestMessageQueryData.
 *
 * @param {MessageEvent<TestMessageQueryData>} event
 */
async function runTestQuery(event) {
  const data = event.data;
  const element = await waitForNode(data.testQuery);
  const result =
      data.property ? JSON.stringify(element[data.property]) : element.tagName;
  const response = {testQueryResult: result};
  event.source.postMessage(response, event.origin);
}

function receiveTestMessage(/** Event */ e) {
  const event = /** @type{MessageEvent<TestMessageQueryData>} */ (e);
  if (event.data.testQuery) {
    runTestQuery(event);
  }
}

window.addEventListener('message', receiveTestMessage, false);

//# sourceURL=guest_query_reciever.js
