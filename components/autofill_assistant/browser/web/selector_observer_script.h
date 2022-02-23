// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SELECTOR_OBSERVER_SCRIPT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SELECTOR_OBSERVER_SCRIPT_H_

namespace autofill_assistant {
namespace selector_observer_script {

// Javascript comments are moved here so that they don't use up space.
//
// (1) selector_id -> element
// (2) selector_id -> element
// (3) element_id -> element
// (4) uses setTimeout so that initialization isn't blocked checking the
//     selectors.
// (5) In case c++ doesn't call terminate()
// (7) A result is serializable by value. Unserializable elements are saved in
//     `pendingElements` and callers must retrieve them by calling
//     `getElements`.
// (7) Hasn't changed so we don't need to update.
// (8) Public API
// (9) Returns a promise that fulfills whenever the status of a selector has
//     changed
// (10) Listened twice for callbacks, send an empty update to the old one.
// (11) Since the polling-based WaitForDom checks less often, this tries to
//      approximate the number of checks the polling-based WaitForDom would have
//      done so that the time they take when element is not found is
//      approximately equivalent.

constexpr char kWaitForChangeScript[] = R"eof(
  const UPDATE = "update";
  const LISTENER_SUPERSEDED = "listenerSuperseded"
  const PAGE_UNLOAD = "pageUnload";
  const TIMEOUT = "timeout";

  let pollingTid = null;
  let hasChanges = false;
  let pendingCallback = null;
  const matchResults = {}; // (1)
  const previousMatchResults = {}; // (2)
  const rootElement = this;
  const ownerWindow = (rootElement.ownerDocument || rootElement).defaultView;
  const pendingElements = {}; // (3)
  let nextElementId = 0;

  const now = () => (new Date()).getTime();

  ownerWindow.addEventListener("unload", () => {
    if (pendingCallback) {
      pendingCallback(PAGE_UNLOAD);
      pendingCallback = null;
    }
  }, true);

  const runSelector = (selector) => {
    const [fn, { args }] = selector;
    const startElement = rootElement.nodeType === Node.DOCUMENT_NODE
      ? rootElement.documentElement
      : rootElement;
    const result = fn.call(startElement, args);
    return Array.isArray(result) ? result[0] : result;
  };

  let checkCount = 0;
  let selectorCheckTime = 0;
  let startTime = null;

  // (11)
  const waitTimeApprox = () => {
    const runTime = (now() - startTime);
    const count = 1 + Math.floor(Math.min(runTime, maxWaitTime) / pollInterval);
    const avgCheckTime = selectorCheckTime / checkCount;
    return runTime - count * avgCheckTime;
  };

  const onChange = () => {
    if (startTime == null) {
      startTime = now();
    }
    checkCount += 1;
    const start = now();
    if (pollingTid) clearTimeout(pollingTid);
    pollingTid = setTimeout(onChange, pollInterval);

    for (const selector of selectors) {
      const node = runSelector(selector);
      const { selectorId } = selector[1];
      if (node !== matchResults[selectorId]) {
        matchResults[selectorId] = node;
        hasChanges = true;
      }
    }

    if (hasChanges && pendingCallback) {
      pendingCallback(UPDATE);
      pendingCallback = null;
    }
    const end = now();
    selectorCheckTime += (end - start);
    if (waitTimeApprox() >= maxWaitTime) {
      if (pendingCallback) {
        pendingCallback(TIMEOUT);
        pendingCallback = null;
      }
      terminate();
    }
  };

  const config = {
    attributes: true,
    childList: true,
    subtree: true,
    characterData: true,
  };
  const observer = new MutationObserver(onChange);
  observer.observe(rootElement, config);
  const terminate = () => {
    observer.disconnect();
    if (pollingTid) clearTimeout(pollingTid);
    clearTimeout(disconnectTid);
  };
  // (4)
  setTimeout(onChange, 0);
  // (5)
  const disconnectTid = setTimeout(terminate, maxRuntime);

  const buildResult = () => {
    // (6)
    const updates = [];
    const result = {
      updates,
      status: UPDATE,
      waitTimeRemaining: (maxWaitTime - waitTimeApprox()) | 0,
      timing: { checkCount, selectorCheckTime },
    };
    const indexes = new WeakMap();

    selectors.forEach(([_, { selectorId, isLeafFrame }]) => {
      const element = matchResults[selectorId];
      if (selectorId in previousMatchResults &&
          previousMatchResults[selectorId] == element) {
        // (7)
        return;
      }
      previousMatchResults[selectorId] = element;

      if (element && !indexes.has(element)) {
        const nextId = ++nextElementId;
        indexes.set(element, nextId);
        pendingElements[nextId] = element;
      }
      const elementId = element ? indexes.get(element) : null;

      updates.push({
        selectorId,
        isLeafFrame,
        elementId,
      });
    });

    return result;
  };

  // (8)
  return {
    terminate,
    addSelectors(selectors) {
      selectors.forEach((selector) => {
        const newSelectorId = selector[1].selectorId;
        delete matchResults[newSelectorId];
        delete previousMatchResults[newSelectorId];
        if (!selectors.find(
            ([_, { selectorId }]) => selectorId == newSelectorId)) {
          selectors.push(selector);
        }
      });
      onChange();
    },
    getElements(elementIds) {
      const result = {};
      elementIds.forEach((id) => {
        result[id] = pendingElements[id];
      });
      return result;
    },
    // (9)
    getChanges() {
      if (hasChanges) {
        hasChanges = false;
        return Promise.resolve(buildResult());
      }

      if (pendingCallback) {
        // (10)
        pendingCallback(LISTENER_SUPERSEDED);
        pendingCallback = null;
      }

      if (startTime && waitTimeApprox() >= maxWaitTime) {
        return Promise.resolve({ status: TIMEOUT });
      }

      return (new Promise((fulfill) => {
        pendingCallback = fulfill;
      })).then((status) => {
        if (status != UPDATE) {
          return { status };
        } else {
          hasChanges = false;
          return buildResult();
        }
      });
    },
  };
)eof";

}  // namespace selector_observer_script
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEB_SELECTOR_OBSERVER_SCRIPT_H_
