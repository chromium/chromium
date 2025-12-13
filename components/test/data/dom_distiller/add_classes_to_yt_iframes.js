// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that the JS file is loaded and the test is running.
suite('AddClassesToYTIFramesTest', function() {
  test('Correctly transforms yt iframes', async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    const iframe = document.createElement('iframe');
    iframe.setAttribute('src', 'https://www.youtube.com');
    document.body.appendChild(container);
    container.appendChild(iframe);
    addClassesToYoutubeIFrames(container);

    const ytContainers = document.getElementsByClassName('youtubeContainer');
    assert.equal(1, ytContainers.length);
    assert.equal(ytContainers[0], iframe.parentElement);
    assert.equal('youtubeIframe', iframe.className);

    document.body.removeChild(container);
  });

  test('Ignores non-yt iframes', async function() {
    // Use a dynamic import since this file is not executed as a module from
    // distilled_page_js_browsertest.cc
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    const iframe = document.createElement('iframe');
    iframe.setAttribute('src', 'https://www.tubeyou.com');
    document.body.appendChild(container);
    container.appendChild(iframe);
    addClassesToYoutubeIFrames(container);


    const ytContainers = document.getElementsByClassName('youtubeContainer');
    assert.equal(0, ytContainers.length);
    assert.notEqual('youtubeIframe', iframe.className);

    document.body.removeChild(container);
  });
});
