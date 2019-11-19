// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ActiveDescendantAttribute = [ 'activeDescendant' ];
var LinkAttributes = [ 'url' ];
var DocumentAttributes = [ 'docUrl',
                           'docTitle',
                           'docLoaded',
                           'docLoadingProgress' ];
var ScrollableAttributes = [ 'scrollX',
                             'scrollXMin',
                             'scrollXMax',
                             'scrollY',
                             'scrollYMin',
                             'scrollYMax' ];
var EditableTextAttributes = [ 'textSelStart',
                               'textSelEnd' ];
var RangeAttributes = [ 'valueForRange',
                        'minValueForRange',
                        'maxValueForRange' ];
var TableAttributes = [ 'tableRowCount',
                        'tableColumnCount',
                        'ariaRowCount',
                        'ariaColumnCount' ];
var TableCellAttributes = [ 'tableCellColumnIndex',
                            'tableCellAriaColumnIndex',
                            'tableCellColumnSpan',
                            'tableCellRowIndex',
                            'tableCellAriaRowIndex',
                            'tableCellRowSpan' ];

var disabledTests = [
  // http://crbug.com/725420
  function testDocumentAndScrollAttributes_flaky() {
    for (var i = 0; i < DocumentAttributes.length; i++) {
      var attribute = DocumentAttributes[i];
      assertTrue(attribute in rootNode,
                 'rootNode should have a ' + attribute + ' attribute');
    }
    for (var i = 0; i < ScrollableAttributes.length; i++) {
      var attribute = ScrollableAttributes[i];
      assertTrue(attribute in rootNode,
                 'rootNode should have a ' + attribute + ' attribute');
    }

    assertEq('Automation Tests - Attributes', rootNode.docTitle);
    assertEq(true, rootNode.docLoaded);
    assertEq(1, rootNode.docLoadingProgress);
    assertEq(0, rootNode.scrollX);
    assertEq(0, rootNode.scrollXMin);
    assertEq(0, rootNode.scrollXMax);
    assertEq(0, rootNode.scrollY);
    assertEq(0, rootNode.scrollYMin);
    assertEq(0, rootNode.scrollYMax);
    chrome.test.succeed();
  }
];

var allTests = [
  function testActiveDescendant() {
    let combobox = rootNode.find({ role: 'textFieldWithComboBox' });
    combobox.addEventListener(EventType.FOCUS, () => {
      assertTrue('activeDescendant' in combobox,
                 'combobox button should have an activedescendant attribute');
      let listbox = rootNode.find({ role: 'listBox' });
      let opt6 = listbox.children[5];
      assertEq(opt6, combobox.activeDescendant);
      chrome.test.succeed();
    }, true);
    combobox.focus();
  },

  function testLinkAttributes() {
    var links = rootNode.findAll({ role: 'link' });
    assertEq(2, links.length);

    var realLink = links[0];
    assertEq('Real link', realLink.name);
    assertTrue('url' in realLink, 'realLink should have a url attribute');
    assertEq('about://blank', realLink.url);

    var ariaLink = links[1];
    assertEq('ARIA link', ariaLink.name);
    assertTrue('url' in ariaLink, 'ariaLink should have an empty url');
    assertEq(undefined, ariaLink.url);
    chrome.test.succeed();
  },

  function testEditableTextAttributes() {
    let textFields = rootNode.findAll({ role: 'textField' });
    assertEq(3, textFields.length);
    for (let textField of textFields) {
      let description = textField.description;
      for (let attribute of EditableTextAttributes) {
        assertTrue(attribute in textField,
                   'textField (' + description + ') should have a ' +
                   attribute + ' attribute');
      }
    }

    let input = textFields[0];
    input.addEventListener(EventType.FOCUS, () => {
      assertEq('text-input', input.name);
      assertEq(2, input.textSelStart);
      assertEq(8, input.textSelEnd);

      let textArea = textFields[1];
      assertEq('textarea', textArea.name);
      for (let attribute of EditableTextAttributes) {
        assertTrue(attribute in textArea,
                   'textArea should have a ' + attribute + ' attribute');
      }

      /* Re-enable the following two assertions once the new selection code is
       * switched on.
      assertEq(0, textArea.textSelStart);
      assertEq(0, textArea.textSelEnd);
      */

      textArea.addEventListener(EventType.FOCUS, () => {
        assertEq(2, textArea.textSelStart);
        assertEq(4, textArea.textSelEnd);

        let ariaTextbox = textFields[2];
        assertEq('textbox-role', ariaTextbox.name);
        assertEq(undefined, ariaTextbox.textSelStart,
                 'ariaTextbox.textSelStart');
        assertEq(undefined, ariaTextbox.textSelEnd, 'ariaTextbox.textSelEnd');
        ariaTextbox.addEventListener(EventType.FOCUS, () => {
          assertEq(undefined, ariaTextbox.textSelStart,
                   'ariaTextbox.textSelStart');
          assertEq(undefined, ariaTextbox.textSelEnd, 'ariaTextbox.textSelEnd');
          chrome.test.succeed();
        }, true);
        ariaTextbox.focus();
      }, true);
      textArea.focus();
    }, true);
    input.focus();
  },

  function testRangeAttributes() {
    var sliders = rootNode.findAll({ role: 'slider' });
    assertEq(2, sliders.length);
    var spinButtons = rootNode.findAll({ role: 'spinButton' });
    assertEq(1, spinButtons.length);
    var progressIndicators = rootNode.findAll({ role: 'progressIndicator' });
    assertEq(2, progressIndicators.length);
    assertEq('progressbar-role', progressIndicators[0].name);
    var scrollBars = rootNode.findAll({ role: 'scrollBar' });
    assertEq(1, scrollBars.length);

    var ranges = sliders.concat(spinButtons, progressIndicators, scrollBars);
    assertEq(6, ranges.length);

    for (var i = 0; i < ranges.length; i++) {
      var range = ranges[i];
      for (var j = 0; j < RangeAttributes.length; j++) {
        var attribute = RangeAttributes[j];
        assertTrue(attribute in range,
                   range.role + ' (' + range.description + ') should have a '
                   + attribute + ' attribute');
      }
    }

    var inputRange = sliders[0];
    assertEq('range-input', inputRange.name);
    assertEq(4, inputRange.valueForRange);
    assertEq(0, inputRange.minValueForRange);
    assertEq(5, inputRange.maxValueForRange);

    var ariaSlider = sliders[1];
    assertEq('slider-role', ariaSlider.name);
    assertEq(7, ariaSlider.valueForRange);
    assertEq(1, ariaSlider.minValueForRange);
    assertEq(10, ariaSlider.maxValueForRange);

    var spinButton = spinButtons[0];
    assertEq(14, spinButton.valueForRange);
    assertEq(1, spinButton.minValueForRange);
    assertEq(31, spinButton.maxValueForRange);

    assertEq(0.9, progressIndicators[0].valueForRange);
    assertEq(0, progressIndicators[0].minValueForRange);
    assertEq(1, progressIndicators[0].maxValueForRange);

    assertEq(0.05, progressIndicators[1].valueForRange);

    assertEq(0, scrollBars[0].valueForRange);
    assertEq(0, scrollBars[0].minValueForRange);
    assertEq(1, scrollBars[0].maxValueForRange);

    chrome.test.succeed();
  },

  function testTableAttributes() {
    var table = rootNode.find({ role: 'grid' });;
    assertEq(3, table.tableRowCount);
    assertEq(103, table.ariaRowCount);
    assertEq(3, table.tableColumnCount);
    assertEq(53, table.ariaColumnCount);

    var row1 = table.firstChild;
    var cell1 = row1.firstChild;
    assertEq(0, cell1.tableCellColumnIndex);
    assertEq(51, cell1.tableCellAriaColumnIndex);
    assertEq(1, cell1.tableCellColumnSpan);
    assertEq(0, cell1.tableCellRowIndex);
    assertEq(101, cell1.tableCellAriaRowIndex);
    assertEq(1, cell1.tableCellRowSpan);

    var cell2 = cell1.nextSibling;
    assertEq(1, cell2.tableCellColumnIndex);
    assertEq(52, cell2.tableCellAriaColumnIndex);
    assertEq(1, cell2.tableCellColumnSpan);
    assertEq(0, cell2.tableCellRowIndex);
    assertEq(101, cell2.tableCellAriaRowIndex);
    assertEq(1, cell2.tableCellRowSpan);

    var cell3 = cell2.nextSibling;
    assertEq(2, cell3.tableCellColumnIndex);
    assertEq(53, cell3.tableCellAriaColumnIndex);
    assertEq(1, cell3.tableCellColumnSpan);
    assertEq(0, cell3.tableCellRowIndex);
    assertEq(101, cell3.tableCellAriaRowIndex);
    assertEq(1, cell3.tableCellRowSpan);

    var row2 = row1.nextSibling;
    var cell4 = row2.firstChild;
    assertEq(0, cell4.tableCellColumnIndex);
    assertEq(51, cell4.tableCellAriaColumnIndex);
    assertEq(2, cell4.tableCellColumnSpan);
    assertEq(1, cell4.tableCellRowIndex);
    assertEq(102, cell4.tableCellAriaRowIndex);
    assertEq(1, cell4.tableCellRowSpan);

    var cell5 = cell4.nextSibling;
    assertEq(2, cell5.tableCellColumnIndex);
    assertEq(53, cell5.tableCellAriaColumnIndex);
    assertEq(1, cell5.tableCellColumnSpan);
    assertEq(1, cell5.tableCellRowIndex);
    assertEq(102, cell5.tableCellAriaRowIndex);
    assertEq(2, cell5.tableCellRowSpan);

    var row3 = row2.nextSibling;
    var cell6 = row3.firstChild;
    assertEq(0, cell6.tableCellColumnIndex);
    assertEq(51, cell6.tableCellAriaColumnIndex);
    assertEq(1, cell6.tableCellColumnSpan);
    assertEq(2, cell6.tableCellRowIndex);
    assertEq(103, cell6.tableCellAriaRowIndex);
    assertEq(1, cell6.tableCellRowSpan);

    var cell7 = cell6.nextSibling;
    assertEq(1, cell7.tableCellColumnIndex);
    assertEq(52, cell7.tableCellAriaColumnIndex);
    assertEq(1, cell7.tableCellColumnSpan);
    assertEq(2, cell7.tableCellRowIndex);
    assertEq(103, cell7.tableCellAriaRowIndex);
    assertEq(1, cell7.tableCellRowSpan);

    chrome.test.succeed();
  },

  function testLangAttribute() {
    var p = rootNode.find({ attributes: { language: 'es-ES' } });
    assertTrue(p !== undefined);
    assertEq('paragraph', p.role);
    chrome.test.succeed();
  },

  function testNoAttributes() {
    var div = rootNode.find({ attributes: { name: 'main' } });
    assertTrue(div !== undefined);
    var allAttributes = [].concat(ActiveDescendantAttribute,
                              LinkAttributes,
                              DocumentAttributes,
                              ScrollableAttributes,
                              EditableTextAttributes,
                              RangeAttributes,
                              TableAttributes,
                              TableCellAttributes);
    for (var attributeAttr in allAttributes) {
      assertFalse(attributeAttr in div);
    }
    chrome.test.succeed();
  },

  function testHtmlAttributes() {
    var editable = rootNode.find({ role: 'textField' });
    assertTrue(editable !== undefined);
    assertEq('text', editable.htmlAttributes.type);
    chrome.test.succeed();
  },

  function testNameFrom() {
    var link = rootNode.find({ role: 'link' });
    assertEq(chrome.automation.NameFromType.CONTENTS, link.nameFrom);
    var textarea = rootNode.find({ attributes: { name: 'textarea' } });
    assertEq(chrome.automation.NameFromType.ATTRIBUTE, textarea.nameFrom);
    chrome.test.succeed();
  },

  function testCheckedAttribute() {
    // Checkbox can use all 3 checked attribute values: true|false|mixed
    var checkTest1 = rootNode.find({ attributes: { name: 'check-test-1' } });
    assertTrue(Boolean(checkTest1));
    assertEq(checkTest1.checked, 'true');

    var checkTest2 = rootNode.find({ attributes: { name: 'check-test-2' } });
    assertTrue(Boolean(checkTest2));
    assertEq(checkTest2.checked, 'false');

    var checkTest3 = rootNode.find({ attributes: { name: 'check-test-3' } });
    assertTrue(Boolean(checkTest3));
    assertEq(checkTest3.checked, 'mixed');

    // Uncheckable nodes have a checked attribute of undefined
    var checkTest4 = rootNode.find({ attributes: { name: 'check-test-4' } });
    assertTrue(Boolean(checkTest4));
    assertEq(checkTest4.checked, undefined);

    // Treeitem can be checked
    var checkTest5 = rootNode.find({ attributes: { name: 'check-test-5' } });
    assertTrue(Boolean(checkTest5));
    assertEq(checkTest5.checked, 'true');

    // button with aria-pressed shows up as checked
    var checkTest6 = rootNode.find({ attributes: { name: 'check-test-6' } });
    assertTrue(Boolean(checkTest6));
    assertEq(checkTest6.checked, 'true');

    chrome.test.succeed();
  },

  function testHtmlTagAttribute() {
    var figure = rootNode.find({ attributes: { htmlTag: 'figure' } });
    assertTrue(Boolean(figure));
    assertEq(figure.htmlTag, 'figure');

    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests, 'attributes.html');
