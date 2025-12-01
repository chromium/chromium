// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that the JS file is loaded and the test is running.
suite('MakeTablesScrollableTest', function() {
  test('Wrap the tables in a scrollable-container', async function() {
    const {assert} = await import('./index.js');

    const container = document.createElement('div');
    container.innerHTML = '<h2>Regular Table</h2>' +
        '<table><tbody><tr><td>Celda 1A</td></tr></tbody></table>' +
        '<h2>Wrapped Table</h2>' +
        '<div class="distilled-scrollable-container">' +
        '<table><tbody><tr><td>Celda 2B</td></tr></tbody></table>' +
        '</div>';

    document.body.appendChild(container);

    const containerClass = 'distilled-scrollable-container';
    const allTables = container.querySelectorAll('table');
    const standardTable = allTables[0];
    const preWrappedTable = allTables[1];

    wrapTables(container);

    assert.equal(
        standardTable.parentElement.tagName, 'DIV',
        'Table must be within a div');
    assert.isTrue(
        standardTable.parentElement.classList.contains(containerClass),
        'The parent must have the distilled-scrollable-container class');

    assert.equal(
        preWrappedTable.parentElement.tagName, 'DIV',
        'The pre-wrapped table should remain the same.');
    assert.isFalse(
        preWrappedTable.parentElement.parentElement.classList.contains(
            containerClass),
        'Avoid double wrapping.');
  });
});
