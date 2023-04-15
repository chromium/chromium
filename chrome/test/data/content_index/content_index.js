// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function WrapFunction(fn) {
  return fn().then(result => `ok - ${result}`)
      .catch(formatError);
}

async function RegisterServiceWorker() {
  await navigator.serviceWorker.register('sw.js', { scope: 'content_index' });
}

async function AddContentForFrame(id) {
  const iframe = document.getElementById('iframe-id');
  await iframe.contentWindow.AddContent(id);
}

async function GetIdsForFrame() {
  const iframe = document.getElementById('iframe-id');
  return await iframe.contentWindow.GetIds();
}

async function AddContent(id) {
  const registration = await navigator.serviceWorker.ready;

  await registration.index.add({
    id: id,
    title: `title ${id}`,
    description: `description ${id} ${Math.random()}`,
    category: 'article',
    icons: [{
      src: '/anchor_download_test.png',
    }],
    url: '/content_index/content_index.html?launch',
  });
}

async function DeleteContent(id) {
  const registration = await navigator.serviceWorker.ready;
  await registration.index.delete(id);
}

async function GetIds() {
  const registration = await navigator.serviceWorker.ready;

  const descriptions = await registration.index.getAll();
  return descriptions.map(d => d.id);
}

async function waitForMessageFromServiceWorker() {
  return await new Promise(r =>
      navigator.serviceWorker.addEventListener('message', e => r(e.data)));
}
