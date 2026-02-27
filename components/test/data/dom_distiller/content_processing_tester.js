// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('ContentProcessing.sanitizeLinks', function() {
  let testContainer;

  setup(function() {
    testContainer = document.createElement('div');
    document.body.appendChild(testContainer);
  });

  teardown(function() {
    document.body.removeChild(testContainer);
    testContainer = null;
  });

  test(
      'sanitizeLinks should keep valid http/https links and open in new tab',
      async function() {
        const {assert} = await import('./index.js');
        testContainer.innerHTML = '<a href="http://example.com">HTTP Link</a>' +
            '<a href="https://example.com">HTTPS Link</a>';
        sanitizeLinks(testContainer);
        const links = testContainer.querySelectorAll('a');
        assert.equal(links.length, 2);
        assert.equal(links[0].getAttribute('href'), 'http://example.com');
        assert.equal(links[0].target, '_blank');
        assert.equal(links[1].getAttribute('href'), 'https://example.com');
        assert.equal(links[1].target, '_blank');
      });

  test('sanitizeLinks should remove javascript links', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<a href="javascript:void(0)">JS Link</a>';
    sanitizeLinks(testContainer);
    const links = testContainer.querySelectorAll('a');
    assert.equal(links.length, 0);
    assert.equal(testContainer.innerHTML, 'JS Link');
  });

  test(
      'sanitizeLinks should keep #in-page links and not open in new tab',
      async function() {
        const {assert} = await import('./index.js');
        testContainer.innerHTML = '<a href="#anchor">Anchor Link</a>';
        sanitizeLinks(testContainer);
        const links = testContainer.querySelectorAll('a');
        assert.equal(links.length, 1);
        assert.equal(links[0].getAttribute('href'), '#anchor');
        // Should NOT be _blank.
        assert.notEqual(links[0].target, '_blank');
      });

  test(
      'sanitizeLinks should keep #in-page links with path-like characters',
      async function() {
        const {assert} = await import('./index.js');
        testContainer.innerHTML = '<a href="#foo/bar">Hash Path Link</a>';
        sanitizeLinks(testContainer);
        const links = testContainer.querySelectorAll('a');
        assert.equal(links.length, 1);
        assert.equal(links[0].getAttribute('href'), '#foo/bar');
        assert.notEqual(links[0].target, '_blank');
      });

  test('sanitizeLinks should remove empty hash links', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML = '<a href="#">Empty Hash</a>';
    sanitizeLinks(testContainer);
    const links = testContainer.querySelectorAll('a');
    assert.equal(links.length, 0);
    assert.equal(testContainer.innerHTML, 'Empty Hash');
  });

  test('sanitizeLinks should remove mailto links', async function() {
    const {assert} = await import('./index.js');
    testContainer.innerHTML =
        '<a href="mailto:user@example.com">Mailto Link</a>';
    sanitizeLinks(testContainer);
    const links = testContainer.querySelectorAll('a');
    assert.equal(links.length, 0);
    assert.equal(testContainer.innerHTML, 'Mailto Link');
  });
});
