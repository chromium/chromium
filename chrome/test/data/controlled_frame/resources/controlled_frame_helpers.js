// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns a Promise containing a new Controlled Frame element that has been
// added to the document and navigated to the specified url. If `url` isn't
// absolute, it will be resolved relative to the `https_origin` parameter given
// to the test.
async function createControlledFrame(url) {
  const params = new URLSearchParams(location.search);
  if (!params.has('https_origin')) {
    throw new Exception('No https_origin query parameter provided');
  }
  const resolvedUrl = new URL(url, params.get('https_origin')).toString();

  const cf = document.createElement('controlledframe');
  await new Promise((resolve, reject) => {
    cf.addEventListener('loadstop', resolve);
    cf.addEventListener('loadabort', reject);
    cf.setAttribute('src', resolvedUrl);
    document.body.appendChild(cf);
  });
  return cf;
}

// Executes `script` in `controlledframe`, and returns a Promise
// containing the result, resolving Promises if necessary.
async function executeAsyncScript(controlledframe, script) {
  const nonce = Math.random().toString(16).slice(2);
  const embeddedHandlerSrc = `(() => {
    const messageHandler = async (message) => {
      if (!('script' in message.data) ||
          !('nonce' in message.data) ||
          message.data.nonce !== '${nonce}') {
        return;
      }
      window.removeEventListener('message', messageHandler);

      const response = { nonce: message.data.nonce };
      try {
        response.result = await eval(message.data.script);
      } catch (e) {
        response.error = e.message;
      }
      message.source.postMessage(response, message.origin);
    };
    window.addEventListener('message', messageHandler);
  })()`;

  const handlerSetupResults =
      await controlledframe.executeScript({code: embeddedHandlerSrc});
  if (handlerSetupResults.length !== 1) {
    throw new Error(
        'executeAsyncScript doesn\'t support multiple frames, ' +
        `${handlerSetupResults.length} were found.`);
  }

  return new Promise((resolve, reject) => {
    const messageHandler = (message) => {
      if (!('nonce' in message.data) || message.data.nonce !== nonce) {
        return;
      }
      window.removeEventListener('message', messageHandler);

      if ('result' in message.data) {
        resolve(message.data.result);
      } else {
        reject(message.data.error || 'Unknown error');
      }
    };
    window.addEventListener('message', messageHandler);

    controlledframe.contentWindow.postMessage({nonce, script}, '*');
  });
}
