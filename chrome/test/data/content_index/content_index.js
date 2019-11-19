// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function WrapFunction(fn) {
  fn().then(result => sendResultToTest(`ok - ${result}`))
      .catch(sendErrorToTest);
}

async function RegisterServiceWorker() {
  await navigator.serviceWorker.register('sw.js', { scope: 'content_index' });
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
    launchUrl: '/content_index/content_index.html?launch',
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
