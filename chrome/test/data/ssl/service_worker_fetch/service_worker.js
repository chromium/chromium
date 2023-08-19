// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The other end of a message channel opened by the page.
let port;

function reply(m) {
  port.postMessage(m);
}

// Performs a fetch to `url` and replies with the result or an encountered
// error.
async function doFetchAndReply(url) {
  try {
    let response = await fetch(url);
    let text = await response.text();
    reply(text);
  } catch (e) {
    reply(`Fetch error: ${e.toString()}`);
  }
}

// Handles an incoming message from the page, which will tell us to perform a
// fetch for a certain URL.
this.onmessage = (e) => {
  port = e.ports[0];
  let url;
  // Validate the message and ensure the URL is a valid URL.
  try {
    if (!e.data.command || e.data.command !== 'fetch' || !e.data.url) {
      reply(`Unexpected message: ${JSON.stringify(e.data)}`);
      return;
    }
    url = new URL(e.data.url);
  } catch (e) {
    reply(`Encountered error: ${e.toString()},` +
          ` message: ${JSON.stringify(e.data)}`);
    return;
  }
  doFetchAndReply(url.href);
};
