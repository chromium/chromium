// Copyright 2020 The Chromium Authors
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

function readField(dictionary, name, defaultValue) {
  return dictionary.get(name) || defaultValue;
}

function respondToShare(event) {
  event.respondWith((async () => {
    const template = await fetch('share.template.html');
    let body = await template.text();
    const formData = (event.request.method === 'POST')
                         ? await event.request.formData()
                         : (new URL(event.request.url)).searchParams;

    body = body.replace('{{method}}', event.request.method)
               .replace('{{type}}', event.request.headers.get('Content-Type'))
               .replace('{{url}}', event.request.url)
               .replace('{{headline}}', readField(formData, 'headline', 'N/A'))
               .replace('{{author}}', readField(formData, 'author', 'N/A'))
               .replace('{{link}}', readField(formData, 'link', 'N/A'));

    const init = {
      status: 200,
      statusText: 'OK',
      headers: {'Content-Type': 'text/html'}
    };

    const file_fields = ['records', 'graphs', 'notes', 'audio', 'image', 'video'];

    let field_index = 0;
    let files = undefined;
    let file_contents = '';
    let file_names = '';
    let index = 0;

    function prepareField() {
      files = formData.getAll(
          file_fields[field_index]);  // sequence of File objects
      file_contents = '';
      file_names = '';
      index = 0;
    }

    prepareField();

    async function progress() {
      while (index === files.length) {
        body = body.replace(
            '{{' + file_fields[field_index] + '}}', file_contents);
        body = body.replace(
            '{{' + file_fields[field_index] + '_filename}}', file_names);

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
        file_names += ' ';
      }
      file_contents += dataFromFileLoaded;
      file_names += files[index].name;
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
