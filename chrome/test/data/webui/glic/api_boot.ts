// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicApiBootMessage, GlicHostRegistry, WithGlicApi} from '/glic/glic_api/glic_api.js';

// Note: Although the actual glic client does something similar, this code is
// test-only.

/** ID of the `<head>` `<script>` element loading `chrome__glic_api.js`. */
const GLIC_API_SCRIPT_ID = 'glic-api-script';

/**
 * Attribute set when a fallback server script executes on older Chrome
 * versions.
 */
const GLIC_API_FALLBACK_LOADED_ATTR = 'data-fallback-loaded';

let registryPromise: Promise<GlicHostRegistry>|undefined;

/**
 * Creates a GlicHostRegistry by waiting for the `<head>` `chrome__glic_api.js`
 * script to load and calling `internalAutoGlicBoot`, falling back to the legacy
 * `'glic-bootstrap'` postMessage handshake if Chrome did not provide the
 * bundle.
 */
export function createGlicHostRegistryOnLoad(): Promise<GlicHostRegistry> {
  registryPromise ??= bootGlicHostRegistry();
  return registryPromise;
}

async function bootGlicHostRegistry(): Promise<GlicHostRegistry> {
  const glicWindow = window as WithGlicApi;
  if (glicWindow.internalAutoGlicBoot) {
    performance.mark('glic-client-bootstrap-received');
    return glicWindow.internalAutoGlicBoot(window);
  }

  const abortController = new AbortController();
  const postMessagePromise =
      waitForBootstrapPostMessage(abortController.signal);
  const scriptBootPromise = waitForGlicApiScriptBoot(abortController.signal);
  if (!scriptBootPromise) {
    return postMessagePromise;
  }

  try {
    return await Promise.race([scriptBootPromise, postMessagePromise]);
  } finally {
    abortController.abort();
  }
}

function waitForGlicApiScriptBoot(abortSignal: AbortSignal):
    Promise<GlicHostRegistry>|undefined {
  const script = document.getElementById(GLIC_API_SCRIPT_ID);
  if (!script || script.hasAttribute(GLIC_API_FALLBACK_LOADED_ATTR)) {
    return undefined;
  }
  return new Promise<GlicHostRegistry>((resolve, reject) => {
    script.addEventListener('load', () => {
      const bootFunc = (window as WithGlicApi).internalAutoGlicBoot;
      if (bootFunc) {
        try {
          performance.mark('glic-client-bootstrap-received');
          resolve(bootFunc(window));
        } catch (error) {
          reject(error);
        }
      }
    }, {once: true, signal: abortSignal});
  });
}

function waitForBootstrapPostMessage(abortSignal: AbortSignal):
    Promise<GlicHostRegistry> {
  return new Promise<GlicHostRegistry>((resolve, reject) => {
    const messageHandler = (event: MessageEvent) => {
      // Important: only accept messages from Chrome browser's glic WebUI.
      if (event.origin !== 'chrome://glic' || event.source === null) {
        return;
      }
      const message = event.data as GlicApiBootMessage | undefined;
      if (!message || message.type !== 'glic-bootstrap') {
        return;
      }
      performance.mark('glic-client-bootstrap-received');
      window.removeEventListener('message', messageHandler);
      try {
        const scriptElement = document.createElement('script');
        scriptElement.textContent = message.glicApiSource;
        document.head.appendChild(scriptElement);
        const bootFunc = (window as WithGlicApi).internalAutoGlicBoot;
        if (!bootFunc) {
          reject(new Error('Glic client import failed.'));
          return;
        }
        resolve(bootFunc(event.source as WindowProxy));
      } catch (error) {
        reject(error);
      }
    };
    window.addEventListener('message', messageHandler, {signal: abortSignal});
  });
}
