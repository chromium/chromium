// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used as dedicated, shared and service worker for
// DlpScopedFileAccessDelegate browser tests.

addEventListener("install", (e) => {
});

addEventListener("fetch", (e) => {
    e.respondWith(
        (async () => {
            const cached = await caches.match(e.request);
            if (cached) {
                return cached;
            }
            return fetch(e.request);
        })());
});

async function work(e) {
    // Given a directory handle, use it to write "This is file content." into
    // the file "write".
    if (e.data.action == 0) {
        let directory = e.data.directory;
        let dirFile = await directory.getFileHandle('test_file.txt', {
            create: true
        });
        let w = await dirFile.createWritable();
        await w.write("This is file content.");
        await w.close();
        return "saved";
    }

    // Given a file handle, use it to write "This is file content." into it.
    if (e.data.action == 1) {
        let file = e.data.file;
        let w = await file.createWritable();
        await w.write("This is file content.");
        await w.close();
        return "saved";
    }

    // Given a file handle, get the file object, slice it by 1 (to create a new
    // file object) and return the sliced content.
    if (e.data.action == 2) {
        try {
            console.log(e.data.file);
            const file = await e.data.file.getFile();
            const copy = file.slice(1);
            const content = await copy.text();
            return content;
        } catch (err) {
            console.log(err.name + ": " + err.message);
            return "Could not read file.";
        }
    }

    // Given a file object, slice it by 1 (to create a new file object) and
    // return the sliced content.
    if (e.data.action == 3) {
        try {
            const file = e.data.file;
            const copy = file.slice(1);
            const content = await copy.text();
            return content;
        } catch (err) {
            console.log(err.name + ": " + err.message);
            return "Could not read file.";
        }
    }

    // Given a directory handle, use it to open the file "test_file.txt", slice
    // it by 1 (to create a new file object) and return the sliced content.
    if (e.data.action == 4) {
        try {
            const directory = e.data.directory;
            const dirFile = await directory.getFileHandle('test_file.txt');
            const file = await dirFile.getFile();
            const copy = file.slice(1);
            const content = await copy.text();
            return content;
        } catch (err) {
            console.log(err.name + ": " + err.message);
            return "Could not read file.";
        }
    }
}

addEventListener('message', async function (e) {
    const ret = await work(e);
    if(e.data.response) {
        e.source.postMessage(ret);
    } else {
        console.log(ret);
    }
});

addEventListener('connect', async function (e) {
    const port = e.ports[0];
    port.addEventListener('message', async function (e) {
        const ret = await work(e);
        port.postMessage(ret);
    });
    port.start();
});