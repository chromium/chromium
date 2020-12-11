// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Promise-based version of FileReader.readAsText.
function readAsFilePromise(fileReader, blob, encoding) {
  return new Promise(resolve => {
    fileReader.onload = e => resolve(e.target.result);
    fileReader.readAsText(blob, encoding);
  });
}

function respondToShare(event) {
  event.respondWith((async () => {
    const template = await fetch('share.template.html');
    let body = await template.text();
    const formData = await event.request.formData();

    body = body.replace('{{headline}}', formData.get('headline'))
               .replace('{{author}}', formData.get('author'))
               .replace('{{link}}', formData.get('link'));

    const init = {
      status: 200,
      statusText: 'OK',
      headers: {'Content-Type': 'text/html'}
    };

    const file_fields = ['records', 'graphs', 'notes'];

    let field_index = 0;
    let files = undefined;
    let file_contents = '';
    let index = 0;

    function prepareField() {
      files = formData.getAll(
          file_fields[field_index]);  // sequence of File objects
      file_contents = '';
      index = 0;
    }

    prepareField();

    async function progress() {
      while (index === files.length) {
        body = body.replace(
            '{{' + file_fields[field_index] + '}}', file_contents);

        ++field_index;
        if (field_index === file_fields.length) {
          return new Response(body, init);
        }
        prepareField();
      }

      const fileReader = new FileReader();
      const dataFromFileLoaded =
          await readAsFilePromise(fileReader, files[index], 'UTF-8');
      if (index > 0) {
        file_contents += ' ';
      }
      file_contents += dataFromFileLoaded;
      index += 1;
      return await progress();
    }

    return await progress();
  })());
}

self.addEventListener('activate', event => {
  event.waitUntil(clients.claim());
});

self.addEventListener('fetch', event => {
  const pathname = (new URL(event.request.url)).pathname;
  if (pathname.endsWith('/share.html')) {
    respondToShare(event);
  } else {
    event.respondWith(fetch(event.request));
  }
});
