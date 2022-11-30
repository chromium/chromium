/*
 * Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Verifies that the layout matches with expectations.
 * @param {Array<string>} rows List of strings where each string indicates the
 *     expected sequence of characters on the corresponding row.
 */
function verifyLayout(rows) {
  var rowIndex = 1;
  rows.forEach(function(sequence) {
    var rowId = 'row' + rowIndex++;
    var first = sequence[0];
    var key = findKey(first, rowId);
    assertTrue(!!key, 'Unable to find "' + first + '" in "' + rowId + '"');
    key = getSoftKeyView(key);
    for (var i = 1; i < sequence.length; i++) {
      var next = key.nextSibling;
      assertTrue(
          !!next, 'Unable to find key to right of "' + sequence[i - 1] + '"');
      assertTrue(
          hasLabel(next, sequence[i]),
          'Unexpected label: expected: "' + sequence[i] + '" to follow "' +
              sequence[i - 1] + '"');
      key = next;
    }
  });
}

/**
 * Validates full layout for a US QWERTY keyboard.
 */
function testFullQwertyLayoutAsync(testDoneCallback) {
  var testCallback = function() {
    var lowercase =
        ['`1234567890-=', 'qwertyuiop[]\\', 'asdfghjkl;\'', 'zxcvbnm,./'];
    var uppercase =
        ['~!@#$%^&*()_+', 'QWERTYUIOP{}', 'ASDFGHJKL:"', 'ZXCVBNM<>?'];
    var view = getActiveView();
    assertTrue(!!view, 'Unable to find active view');
    assertEquals('us', view.id, 'Expecting full layout');
    verifyLayout(lowercase);
    mockTap(findKeyById('ShiftLeft'));
    verifyLayout(uppercase);
    mockTap(findKeyById('ShiftRight'));
    verifyLayout(lowercase);
    testDoneCallback();
  };
  var config =
      {keyset: 'us', languageCode: 'en', passwordLayout: 'us', name: 'English'};
  onKeyboardReady(testCallback, config);
}

/**
 * Validates compact layout for a US QWERTY keyboard.
 */
function testCompactQwertyLayoutAsync(testDoneCallback) {
  var testCallback = function() {
    var lowercase = ['qwertyuiop', 'asdfghjkl', 'zxcvbnm!?'];
    var uppercase = ['QWERTYUIOP', 'ASDFGHJKL', 'ZXCVBNM!?'];
    var symbol = ['1234567890', '@#$%&-+()', '\\=*"\':;!?'];
    var more = [
      '~`|', '\u00a3\u00a2\u20ac\u00a5^\u00b0={}',
      '\\\u00a9\u00ae\u2122\u2105[]\u00a1\u00bf'
    ];
    var view = getActiveView();
    assertTrue(!!view, 'Unable to find active view');
    assertEquals('us-compact-qwerty', view.id, 'Expecting compact layout');
    verifyLayout(lowercase);
    mockTap(findKeyById('ShiftLeft'));
    verifyLayout(uppercase);
    // Keyset views for symbol and more on the compact layout are lazy
    // initialized. Wait for view creation to complete before continuing the
    // test.
    onKeysetsReady(['us.compact.symbol', 'us.compact.more'], function() {
      onSwitchToKeyset('us.compact.symbol', function() {
        assertEquals(
            'us-compact-symbol', getActiveView().id, 'Expecting symbol layout');
        verifyLayout(symbol);
        onSwitchToKeyset('us.compact.more', function() {
          assertEquals(
              'us-compact-more', getActiveView().id,
              'Expecting more symbols layout');
          verifyLayout(more);
          onSwitchToKeyset('us.compact.qwerty', function() {
            assertEquals(
                'us-compact-qwerty', getActiveView().id,
                'Expecting compact text layout');
            verifyLayout(lowercase);
            testDoneCallback();
          });
          mockTap(findKey('abc'));
        });
        mockTap(findKey('~[<'));
      });
      mockTap(findKey('?123'));
    });
  };
  var config = {
    keyset: 'us.compact.qwerty',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English'
  };
  onKeyboardReady(testCallback, config);
}

/**
 * Tests that handwriting support is disabled by default.
 */
function testHandwritingSupportAsync(testDoneCallback) {
  onKeyboardReady(function() {
    var menu = document.querySelector('.inputview-menu-view');
    assertTrue(!!menu, 'Unable to access keyboard menu');
    assertEquals(
        'none', getComputedStyle(menu)['display'],
        'Menu should not be visible until activated');
    mockTap(findKeyById('Menu'));
    assertEquals(
        'block', getComputedStyle(menu)['display'],
        'Menu should be visible once activated');
    var hwt = menu.querySelector('#handwriting');
    assertFalse(!!hwt, 'Handwriting should be disabled by default');
    testDoneCallback();
  });
}

/**
 * Validates Handwriting layout. Though handwriting is disabled for the system
 * VK, the layout is still available and useful for testing expected behavior of
 * the IME-VKs since the codebase is shared.
 */
function testHandwritingLayoutAsync(testDoneCallback) {
  var compactKeysets = [
    'us.compact.qwerty', 'us.compact.symbol', 'us.compact.more',
    'us.compact.numberpad'
  ];
  var testCallback = function() {
    // Non-active keysets are lazy loaded in order to reduce latency before
    // the virtual keyboard is shown. Wait until the load is complete to
    // continue testing.
    onKeysetsReady(compactKeysets, function() {
      var menu = document.querySelector('.inputview-menu-view');
      assertEquals(
          'none', getComputedStyle(menu).display,
          'Menu should be hidden initially');
      mockTap(findKeyById('Menu'));
      assertFalse(
          menu.hidden,
          'Menu should be visible after tapping menu toggle button');
      var menuBounds = menu.getBoundingClientRect();
      assertTrue(
          menuBounds.width > 0 && menuBounds.height > 0,
          'Expect non-zero menu bounds.');
      var hwtSelect = menu.querySelector('#handwriting');
      assertTrue(!!hwtSelect, 'Handwriting should be available for testing');
      var hwtSelectBounds = hwtSelect.getBoundingClientRect();
      assertTrue(
          hwtSelectBounds.width > 0 && hwtSelectBounds.height > 0,
          'Expect non-zero size for hwt select button.');
      onSwitchToKeyset('hwt', function() {
        // The tests below for handwriting part is for material design.
        var view = getActiveView();
        assertEquals('hwt', view.id, 'Handwriting layout is not active.');
        var hwtCanvasView = view.querySelector('#canvasView');
        assertTrue(!!hwtCanvasView, 'Unable to find canvas view');
        var panelView = document.getElementById('panelView');
        assertTrue(!!panelView, 'Unable to find panel view');
        var backButton = panelView.querySelector('#backToKeyboard');
        assertTrue(!!backButton, 'Unable to find back button.');
        onSwitchToKeyset('us.compact.qwerty', function() {
          assertEquals(
              'us-compact-qwerty', getActiveView().id,
              'compact layout is not active.');
          testDoneCallback();
        });
        mockTap(backButton);
      });
      mockTap(hwtSelect);
    });
  };
  var config = {
    keyset: 'us.compact.qwerty',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English',
    options: {enableHwtForTesting: true}
  };
  onKeyboardReady(testCallback, config);
}

/**
 * Test that IME switching from the InputView menu works.
 */
function testKeyboardSwitchIMEAsync(testDoneCallback) {
  var testCallback = function() {
    // Ensure that the menu key is present and displays the menu when pressed.
    var menu = document.querySelector('.inputview-menu-view');
    assertEquals(
        'none', getComputedStyle(menu).display,
        'Menu should be hidden initially');
    mockTap(findKeyById('Menu'));
    assertFalse(
        menu.hidden, 'Menu should be visible after tapping menu toggle button');
    var menuBounds = menu.getBoundingClientRect();
    assertTrue(
        menuBounds.width > 0 && menuBounds.height > 0,
        'Expect non-zero menu bounds.');

    var imes = menu.querySelectorAll('.inputview-menu-list-indicator-name');
    assertEquals(3, imes.length, 'Unexpected number of IMEs in menu view.');
    assertEquals('US', imes[0].innerText, 'Unexpected IMEs in menu view');
    assertEquals('Fr', imes[1].innerText, 'Unexpected IMEs in menu view');
    assertEquals('De', imes[2].innerText, 'Unexpected IMEs in menu view');

    // Expect a call to change to the German IME.
    chrome.inputMethodPrivate.setCurrentInputMethod.addExpectation('de');

    // Select the German IME and ensure that the menu is dismissed.
    mockTap(imes[2]);
    assertEquals('none', menu.style.display, 'Menu didn\'t hide on switch.');

    testDoneCallback();
  };
  var config = {
    keyset: 'us.compact.qwerty',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English'
  };
  // Explicitly set up the available input methods.
  chrome.inputMethodPrivate.getInputMethods.setCallbackData([
    {id: 'us', name: 'US Keyboard', indicator: 'US'},
    {id: 'fr', name: 'French Keyboard', indicator: 'Fr'},
    {id: 'de', name: 'German Keyboard', indicator: 'De'}
  ]);
  onKeyboardReady(testCallback, config);
}
