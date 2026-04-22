// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ActiveDescendantAttribute = ['activeDescendant'];
const LinkAttributes = ['url'];
const DocumentAttributes =
    ['docUrl', 'docTitle', 'docLoaded', 'docLoadingProgress'];
const ScrollableAttributes = [
  'scrollX',
  'scrollXMin',
  'scrollXMax',
  'scrollY',
  'scrollYMin',
  'scrollYMax',
];
const EditableTextAttributes = ['textSelStart', 'textSelEnd'];
const RangeAttributes =
    ['valueForRange', 'minValueForRange', 'maxValueForRange'];
const TableAttributes =
    ['tableRowCount', 'tableColumnCount', 'ariaRowCount', 'ariaColumnCount'];
const TableCellAttributes = [
  'tableCellColumnIndex',
  'tableCellAriaColumnIndex',
  'tableCellColumnSpan',
  'tableCellRowIndex',
  'tableCellAriaRowIndex',
  'tableCellRowSpan',
];

const disabledTests = [
  // http://crbug.com/41320992
  function testDocumentAndScrollAttributes_flaky() {
    for (let i = 0; i < DocumentAttributes.length; i++) {
      const attribute = DocumentAttributes[i];
      assertTrue(
          attribute in rootNode,
          `rootNode should have a ${attribute} attribute`);
    }
    for (let i = 0; i < ScrollableAttributes.length; i++) {
      const attribute = ScrollableAttributes[i];
      assertTrue(
          attribute in rootNode,
          `rootNode should have a ${attribute} attribute`);
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
  },
];

const allTests = [
  function testActiveDescendant() {
    const combobox = rootNode.find({role: 'textFieldWithComboBox'});
    assertTrue(combobox.isComboBox);
    combobox.addEventListener(EventType.FOCUS, () => {
      assertTrue(
          'activeDescendant' in combobox,
          'combobox button should have an activedescendant attribute');
      const listbox = rootNode.find({role: 'listBox'});
      assertFalse(listbox.isComboBox);
      const opt6 = listbox.children[5];
      assertEq(opt6, combobox.activeDescendant);
      chrome.test.succeed();
    }, true);
    combobox.focus();
  },

  function testLinkAttributes() {
    const links = rootNode.findAll({role: 'link'});
    assertEq(2, links.length);

    const realLink = links[0];
    assertEq('Real link', realLink.name);
    assertTrue('url' in realLink, 'realLink should have a url attribute');
    assertEq('about://blank', realLink.url);

    const ariaLink = links[1];
    assertEq('ARIA link', ariaLink.name);
    assertTrue('url' in ariaLink, 'ariaLink should have an empty url');
    assertEq(undefined, ariaLink.url);
    chrome.test.succeed();
  },

  function testEditableTextAttributes() {
    const textFields = rootNode.findAll({role: 'textField'});
    assertEq(3, textFields.length);
    for (const textField of textFields) {
      const description = textField.description;
      for (const attribute of EditableTextAttributes) {
        assertTrue(
            attribute in textField,
            `textField (${description}) should have a ` +
                `${attribute} attribute`);
      }
    }

    const input = textFields[0];
    input.addEventListener(EventType.FOCUS, () => {
      assertEq('text-input', input.name);
      assertEq(2, input.textSelStart);
      assertEq(8, input.textSelEnd);

      const textArea = textFields[1];
      assertEq('textarea', textArea.name);
      for (const attribute of EditableTextAttributes) {
        assertTrue(
            attribute in textArea,
            `textArea should have a ${attribute} attribute`);
      }

      /* Re-enable the following two assertions once the new selection code is
       * switched on.
      assertEq(0, textArea.textSelStart);
      assertEq(0, textArea.textSelEnd);
      */

      textArea.addEventListener(EventType.FOCUS, () => {
        assertEq(2, textArea.textSelStart);
        assertEq(4, textArea.textSelEnd);

        const ariaTextbox = textFields[2];
        assertEq('textbox-role', ariaTextbox.name);
        assertEq(
            undefined, ariaTextbox.textSelStart, 'ariaTextbox.textSelStart');
        assertEq(undefined, ariaTextbox.textSelEnd, 'ariaTextbox.textSelEnd');
        ariaTextbox.addEventListener(EventType.FOCUS, () => {
          assertEq(
              undefined, ariaTextbox.textSelStart, 'ariaTextbox.textSelStart');
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
    const sliders = rootNode.findAll({role: 'slider'});
    assertEq(2, sliders.length);
    const spinButtons = rootNode.findAll({role: 'spinButton'});
    assertEq(1, spinButtons.length);
    const progressIndicators = rootNode.findAll({role: 'progressIndicator'});
    assertEq(2, progressIndicators.length);
    assertEq('progressbar-role', progressIndicators[0].name);
    const scrollBars = rootNode.findAll({role: 'scrollBar'});
    assertEq(1, scrollBars.length);

    const ranges = sliders.concat(spinButtons, progressIndicators, scrollBars);
    assertEq(6, ranges.length);

    for (let i = 0; i < ranges.length; i++) {
      const range = ranges[i];
      for (let j = 0; j < RangeAttributes.length; j++) {
        const attribute = RangeAttributes[j];
        assertTrue(
            attribute in range,
            `${range.role} (${range.description}) should have a ` +
                `${attribute} attribute`);
      }
    }

    const inputRange = sliders[0];
    assertEq('range-input', inputRange.name);
    assertEq(4, inputRange.valueForRange);
    assertEq(0, inputRange.minValueForRange);
    assertEq(5, inputRange.maxValueForRange);

    const ariaSlider = sliders[1];
    assertEq('slider-role', ariaSlider.name);
    assertEq(7, ariaSlider.valueForRange);
    assertEq(1, ariaSlider.minValueForRange);
    assertEq(10, ariaSlider.maxValueForRange);

    const spinButton = spinButtons[0];
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
    const table = rootNode.find({role: 'grid'});
    assertEq(3, table.tableRowCount);
    assertEq(103, table.ariaRowCount);
    assertEq(3, table.tableColumnCount);
    assertEq(53, table.ariaColumnCount);

    const row1 = table.firstChild;
    const cell1 = row1.firstChild;
    assertEq(0, cell1.tableCellColumnIndex);
    assertEq(51, cell1.tableCellAriaColumnIndex);
    assertEq(1, cell1.tableCellColumnSpan);
    assertEq(0, cell1.tableCellRowIndex);
    assertEq(101, cell1.tableCellAriaRowIndex);
    assertEq(1, cell1.tableCellRowSpan);

    const cell2 = cell1.nextSibling;
    assertEq(1, cell2.tableCellColumnIndex);
    assertEq(52, cell2.tableCellAriaColumnIndex);
    assertEq(1, cell2.tableCellColumnSpan);
    assertEq(0, cell2.tableCellRowIndex);
    assertEq(101, cell2.tableCellAriaRowIndex);
    assertEq(1, cell2.tableCellRowSpan);

    const cell3 = cell2.nextSibling;
    assertEq(2, cell3.tableCellColumnIndex);
    assertEq(53, cell3.tableCellAriaColumnIndex);
    assertEq(1, cell3.tableCellColumnSpan);
    assertEq(0, cell3.tableCellRowIndex);
    assertEq(101, cell3.tableCellAriaRowIndex);
    assertEq(1, cell3.tableCellRowSpan);

    const row2 = row1.nextSibling;
    const cell4 = row2.firstChild;
    assertEq(0, cell4.tableCellColumnIndex);
    assertEq(51, cell4.tableCellAriaColumnIndex);
    assertEq(2, cell4.tableCellColumnSpan);
    assertEq(1, cell4.tableCellRowIndex);
    assertEq(102, cell4.tableCellAriaRowIndex);
    assertEq(1, cell4.tableCellRowSpan);

    const cell5 = cell4.nextSibling;
    assertEq(2, cell5.tableCellColumnIndex);
    assertEq(53, cell5.tableCellAriaColumnIndex);
    assertEq(1, cell5.tableCellColumnSpan);
    assertEq(1, cell5.tableCellRowIndex);
    assertEq(102, cell5.tableCellAriaRowIndex);
    assertEq(2, cell5.tableCellRowSpan);

    const row3 = row2.nextSibling;
    const cell6 = row3.firstChild;
    assertEq(0, cell6.tableCellColumnIndex);
    assertEq(51, cell6.tableCellAriaColumnIndex);
    assertEq(1, cell6.tableCellColumnSpan);
    assertEq(2, cell6.tableCellRowIndex);
    assertEq(103, cell6.tableCellAriaRowIndex);
    assertEq(1, cell6.tableCellRowSpan);

    const cell7 = cell6.nextSibling;
    assertEq(1, cell7.tableCellColumnIndex);
    assertEq(52, cell7.tableCellAriaColumnIndex);
    assertEq(1, cell7.tableCellColumnSpan);
    assertEq(2, cell7.tableCellRowIndex);
    assertEq(103, cell7.tableCellAriaRowIndex);
    assertEq(1, cell7.tableCellRowSpan);

    chrome.test.succeed();
  },

  function testLangAttribute() {
    const p = rootNode.find({attributes: {language: 'es-ES'}});
    assertTrue(p !== undefined);
    assertEq('paragraph', p.role);
    chrome.test.succeed();
  },

  function testNoAttributes() {
    const div = rootNode.find({attributes: {name: 'main'}});
    assertTrue(div !== undefined);
    const allAttributes = [].concat(
        ActiveDescendantAttribute, LinkAttributes, DocumentAttributes,
        ScrollableAttributes, EditableTextAttributes, RangeAttributes,
        TableAttributes, TableCellAttributes);
    for (const attributeAttr in allAttributes) {
      assertFalse(attributeAttr in div);
    }
    chrome.test.succeed();
  },

  function testNameFrom() {
    const link = rootNode.find({role: 'link'});
    assertEq(chrome.automation.NameFromType.CONTENTS, link.nameFrom);
    const textarea = rootNode.find({attributes: {name: 'textarea'}});
    assertEq(chrome.automation.NameFromType.ATTRIBUTE, textarea.nameFrom);
    chrome.test.succeed();
  },

  function testCheckedAttribute() {
    // Checkbox can use all 3 checked attribute values: true|false|mixed
    const checkTest1 = rootNode.find({attributes: {name: 'check-test-1'}});
    assertTrue(checkTest1.isCheckBox);
    assertTrue(Boolean(checkTest1));
    assertEq(checkTest1.checked, 'true');

    const checkTest2 = rootNode.find({attributes: {name: 'check-test-2'}});
    assertTrue(Boolean(checkTest2));
    assertEq(checkTest2.checked, 'false');

    const checkTest3 = rootNode.find({attributes: {name: 'check-test-3'}});
    assertTrue(Boolean(checkTest3));
    assertEq(checkTest3.checked, 'mixed');

    // Uncheckable nodes have a checked attribute of undefined
    const checkTest4 = rootNode.find({attributes: {name: 'check-test-4'}});
    assertTrue(Boolean(checkTest4));
    assertEq(checkTest4.checked, undefined);

    // Treeitem can be checked
    const checkTest5 = rootNode.find({attributes: {name: 'check-test-5'}});
    assertTrue(Boolean(checkTest5));
    assertEq(checkTest5.checked, 'true');

    // button with aria-pressed shows up as checked
    const checkTest6 = rootNode.find({attributes: {name: 'check-test-6'}});
    assertTrue(Boolean(checkTest6));
    assertEq(checkTest6.checked, 'true');

    chrome.test.succeed();
  },

  function testHtmlTagAttribute() {
    const figure = rootNode.find({attributes: {htmlTag: 'figure'}});
    assertTrue(Boolean(figure));
    assertEq(figure.htmlTag, 'figure');

    chrome.test.succeed();
  },

  function testIsImageAttribute() {
    const img = rootNode.find({role: 'image'});
    assertTrue(Boolean(img));
    assertTrue(img.isImage);

    chrome.test.succeed();
  },
];

setUpAndRunTabsTests(allTests, 'attributes.html');
