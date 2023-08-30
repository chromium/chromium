// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';

/**
 * Shared function for InferLabelFromPrevious() and InferLabelFromNext().
 *
 * It is based on the logic in
 *     string16 InferLabelFromSibling(const WebFormControlElement& element,
 *                                    bool forward)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @param {boolean} forward whether to search for the next or previous element.
 * @return {string} The label of element or an empty string if there is no
 *                  sibling or no label.
 */
__gCrWeb.fill.inferLabelFromSibling = function(element, forward) {
  let inferredLabel = '';
  let sibling = element;
  if (!sibling) {
    return '';
  }

  while (true) {
    if (forward) {
      sibling = sibling.nextSibling;
    } else {
      sibling = sibling.previousSibling;
    }

    if (!sibling) {
      break;
    }

    // Skip over comments.
    const nodeType = sibling.nodeType;
    if (nodeType === Node.COMMENT_NODE) {
      continue;
    }

    // Otherwise, only consider normal HTML elements and their contents.
    if (nodeType !== Node.TEXT_NODE && nodeType !== Node.ELEMENT_NODE) {
      break;
    }

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    if (nodeType === Node.TEXT_NODE || __gCrWeb.fill.hasTagName(sibling, 'b') ||
        __gCrWeb.fill.hasTagName(sibling, 'strong') ||
        __gCrWeb.fill.hasTagName(sibling, 'span') ||
        __gCrWeb.fill.hasTagName(sibling, 'font')) {
      const value = __gCrWeb.fill.findChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      const addSpace = nodeType === Node.TEXT_NODE && value.length === 0;
      if (forward) {
        inferredLabel = __gCrWeb.fill.combineAndCollapseWhitespace(
            inferredLabel, value, addSpace);
      } else {
        inferredLabel = __gCrWeb.fill.combineAndCollapseWhitespace(
            value, inferredLabel, addSpace);
      }
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    const trimmedLabel = inferredLabel.trim();
    if (trimmedLabel.length > 0) {
      break;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (__gCrWeb.fill.hasTagName(sibling, 'img') ||
        __gCrWeb.fill.hasTagName(sibling, 'br')) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    if (__gCrWeb.fill.hasTagName(sibling, 'p') ||
        __gCrWeb.fill.hasTagName(sibling, 'label')) {
      inferredLabel = __gCrWeb.fill.findChildText(sibling);
    }
    break;
  }
  return inferredLabel.trim();
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a previous sibling of |element|,
 * e.g. Some Text <input ...>
 * or   Some <span>Text</span> <input ...>
 * or   <p>Some Text</p><input ...>
 * or   <label>Some Text</label> <input ...>
 * or   Some Text <img><input ...>
 * or   <b>Some Text</b><br/> <input ...>.
 *
 * It is based on the logic in
 *     string16 InferLabelFromPrevious(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromPrevious = function(element) {
  return __gCrWeb.fill.inferLabelFromSibling(element, false);
};

/**
 * Same as InferLabelFromPrevious(), but in the other direction.
 * Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
 *
 * It is based on the logic in
 *     string16 InferLabelFromNext(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromNext = function(element) {
  return __gCrWeb.fill.inferLabelFromSibling(element, true);
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the placeholder attribute.
 *
 * It is based on the logic in
 *     string16 InferLabelFromPlaceholder(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromPlaceholder = function(element) {
  if (!element) {
    return '';
  }

  return element.placeholder || element.getAttribute('placeholder') || '';
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the value attribute when it is present and user has not typed in (if
 * element's value attribute is same as the element's value).
 *
 * It is based on the logic in
 *     string16 InferLabelFromValueAttr(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.InferLabelFromValueAttr = function(element) {
  if (!element || !element.value || !element.hasAttribute('value') ||
      element.value !== element.getAttribute('value')) {
    return '';
  }

  return element.value;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * enclosing list item, e.g.
 *     <li>Some Text<input ...><input ...><input ...></li>
 *
 * It is based on the logic in
 *     string16 InferLabelFromListItem(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromListItem = function(element) {
  if (!element) {
    return '';
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.fill.hasTagName(parentNode, 'li')) {
    parentNode = parentNode.parentNode;
  }

  if (parentNode && __gCrWeb.fill.hasTagName(parentNode, 'li')) {
    return __gCrWeb.fill.findChildText(parentNode);
  }

  return '';
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * surrounding table structure,
 * e.g. <tr><td>Some Text</td><td><input ...></td></tr>
 * or   <tr><th>Some Text</th><td><input ...></td></tr>
 * or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
 * or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
 *
 * It is based on the logic in
 *    string16 InferLabelFromTableColumn(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromTableColumn = function(element) {
  if (!element) {
    return '';
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.fill.hasTagName(parentNode, 'td')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return '';
  }

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  let inferredLabel = '';
  let previous = parentNode.previousSibling;
  while (inferredLabel.length === 0 && previous) {
    if (__gCrWeb.fill.hasTagName(previous, 'td') ||
        __gCrWeb.fill.hasTagName(previous, 'th')) {
      inferredLabel = __gCrWeb.fill.findChildText(previous);
    }
    previous = previous.previousSibling;
  }

  return inferredLabel;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * surrounding table structure,
 * e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
 *
 * If there are multiple cells and the row with the input matches up with the
 * previous row, then look for a specific cell within the previous row.
 * e.g. <tr><td>Input 1 label</td><td>Input 2 label</td></tr>
 *  <tr><td><input name="input 1"></td><td><input name="input2"></td></tr>
 *
 * It is based on the logic in
 *     string16 InferLabelFromTableRow(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromTableRow = function(element) {
  if (!element) {
    return '';
  }

  let cell = element.parentNode;
  while (cell) {
    if (cell.nodeType === Node.ELEMENT_NODE &&
        __gCrWeb.fill.hasTagName(cell, 'td')) {
      break;
    }
    cell = cell.parentNode;
  }

  // Not in a cell - bail out.
  if (!cell) {
    return '';
  }

  // Count the cell holding |element|.
  let cellCount = cell.colSpan;
  let cellPosition = 0;
  let cellPositionEnd = cellCount - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  let cellIterator = cell.previousSibling;
  while (cellIterator) {
    if (cellIterator.nodeType === Node.ELEMENT_NODE &&
        __gCrWeb.fill.hasTagName(cellIterator, 'td')) {
      cellPosition += cellIterator.colSpan;
    }
    cellIterator = cellIterator.previousSibling;
  }

  // Count cells to the right.
  cellIterator = cell.nextSibling;
  while (cellIterator) {
    if (cellIterator.nodeType === Node.ELEMENT_NODE &&
        __gCrWeb.fill.hasTagName(cellIterator, 'td')) {
      cellCount += cellIterator.colSpan;
    }
    cellIterator = cellIterator.nextSibling;
  }

  // Combine left + right.
  cellCount += cellPosition;
  cellPositionEnd += cellPosition;

  // Find the current row.
  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.fill.hasTagName(parentNode, 'tr')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return '';
  }

  // Now find the previous row.
  let rowIt = parentNode.previousSibling;
  while (rowIt) {
    if (rowIt.nodeType === Node.ELEMENT_NODE &&
        __gCrWeb.fill.hasTagName(parentNode, 'tr')) {
      break;
    }
    rowIt = rowIt.previousSibling;
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (rowIt) {
    let matchingCell = null;
    let prevRowCount = 0;
    let prevRowIt = rowIt.firstChild;
    while (prevRowIt) {
      if (prevRowIt.nodeType === Node.ELEMENT_NODE) {
        if (__gCrWeb.fill.hasTagName(prevRowIt, 'td') ||
            __gCrWeb.fill.hasTagName(prevRowIt, 'th')) {
          const span = prevRowIt.colSpan;
          const prevRowCountEnd = prevRowCount + span - 1;
          if (prevRowCount === cellPosition &&
              prevRowCountEnd === cellPositionEnd) {
            matchingCell = prevRowIt;
          }
          prevRowCount += span;
        }
      }
      prevRowIt = prevRowIt.nextSibling;
    }
    if (cellCount === prevRowCount && matchingCell) {
      const inferredLabel = __gCrWeb.fill.findChildText(matchingCell);
      if (inferredLabel.length > 0) {
        return inferredLabel;
      }
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  let inferredLabel = '';
  let previous = parentNode.previousSibling;
  while (inferredLabel.length === 0 && previous) {
    if (__gCrWeb.fill.hasTagName(previous, 'tr')) {
      inferredLabel = __gCrWeb.fill.findChildText(previous);
    }
    previous = previous.previousSibling;
  }
  return inferredLabel;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * an enclosing label.
 * e.g. <label>Some Text<span><input ...></span></label>
 *
 * It is based on the logic in
 *    string16 InferLabelFromEnclosingLabel(
 *        const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromEnclosingLabel = function(element) {
  if (!element) {
    return '';
  }
  let node = element.parentNode;
  while (node && !__gCrWeb.fill.hasTagName(node, 'label')) {
    node = node.parentNode;
  }
  if (node) {
    return __gCrWeb.fill.findChildText(node);
  }
  return '';
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a surrounding div table,
 * e.g. <div>Some Text<span><input ...></span></div>
 * e.g. <div>Some Text</div><div><input ...></div>
 *
 * Contrary to the other InferLabelFrom* functions, this functions walks up
 * the DOM tree from the original input, instead of down from the surrounding
 * tag. While doing so, if a <label> or text node sibling are found along the
 * way, a label is inferred from them directly. For example, <div>First
 * name<div><input></div>Last name<div><input></div></div> infers "First name"
 * and "Last name" for the two inputs, respectively, by picking up the text
 * nodes on the way to the surrounding div. Without doing so, the label of both
 * inputs becomes "First nameLast name".
 *
 * It is based on the logic in
 *    string16 InferLabelFromDivTable(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromDivTable = function(element) {
  if (!element) {
    return '';
  }

  let node = element.parentNode;
  let lookingForParent = true;
  const divsToSkip = [];

  // Search the sibling and parent <div>s until we find a candidate label.
  let inferredLabel = '';
  while (inferredLabel.length === 0 && node) {
    if (__gCrWeb.fill.hasTagName(node, 'div')) {
      if (lookingForParent) {
        inferredLabel =
            __gCrWeb.fill.findChildTextWithIgnoreList(node, divsToSkip);
      } else {
        inferredLabel = __gCrWeb.fill.findChildText(node);
      }
      // Avoid sibling DIVs that contain autofillable fields.
      if (!lookingForParent && inferredLabel.length > 0) {
        const resultElement = node.querySelector('input, select, textarea');
        if (resultElement) {
          inferredLabel = '';
          let addDiv = true;
          for (let i = 0; i < divsToSkip.length; ++i) {
            if (node === divsToSkip[i]) {
              addDiv = false;
              break;
            }
          }
          if (addDiv) {
            divsToSkip.push(node);
          }
        }
      }

      lookingForParent = false;
    } else if (!lookingForParent) {
      // Infer a label from text nodes and unassigned <label> siblings.
      if (__gCrWeb.fill.hasTagName(node, 'label') && !node.control) {
        inferredLabel = __gCrWeb.fill.findChildText(node);
      } else if (node.nodeType === Node.TEXT_NODE) {
        inferredLabel = __gCrWeb.fill.nodeValue(node).trim();
      }
    } else if (__gCrWeb.fill.isTraversableContainerElement(node)) {
      // If the element is in a non-div container, its label most likely is too.
      break;
    }

    if (!node.previousSibling) {
      // If there are no more siblings, continue walking up the tree.
      lookingForParent = true;
    }

    if (lookingForParent) {
      node = node.parentNode;
    } else {
      node = node.previousSibling;
    }
  }

  return inferredLabel;
};

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a surrounding definition list,
 * e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
 * e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
 *
 * It is based on the logic in
 *    string16 InferLabelFromDefinitionList(
 *        const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The label of element.
 */
__gCrWeb.fill.inferLabelFromDefinitionList = function(element) {
  if (!element) {
    return '';
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !__gCrWeb.fill.hasTagName(parentNode, 'dd')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode || !__gCrWeb.fill.hasTagName(parentNode, 'dd')) {
    return '';
  }

  // Skip by any intervening text nodes.
  let previous = parentNode.previousSibling;
  while (previous && previous.nodeType === Node.TEXT_NODE) {
    previous = previous.previousSibling;
  }

  if (!previous || !__gCrWeb.fill.hasTagName(previous, 'dt')) {
    return '';
  }

  return __gCrWeb.fill.findChildText(previous);
};

/**
 * Infers corresponding label for |element| from surrounding context in the DOM,
 * e.g. the contents of the preceding <p> tag or text element.
 *
 * It is based on the logic in
 *    string16 InferLabelForElement(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return {string} The inferred label of element, or '' if none could be found.
 */
__gCrWeb.fill.inferLabelForElement = function(element) {
  let inferredLabel;
  if (__gCrWeb.fill.isCheckableElement(element)) {
    inferredLabel = __gCrWeb.fill.inferLabelFromNext(element);
    if (__gCrWeb.fill.IsLabelValid(inferredLabel)) {
      return inferredLabel;
    }
  }

  inferredLabel = __gCrWeb.fill.inferLabelFromPrevious(element);
  if (__gCrWeb.fill.IsLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  // If we didn't find a label, check for the placeholder case.
  inferredLabel = __gCrWeb.fill.inferLabelFromPlaceholder(element);
  if (__gCrWeb.fill.IsLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  // If we didn't find a placeholder, check for the aria-label case.
  inferredLabel = __gCrWeb.fill.getAriaLabel(element);
  if (__gCrWeb.fill.IsLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  // For all other searches that involve traversing up the tree, the search
  // order is based on which tag is the closest ancestor to |element|.
  const tagNames = __gCrWeb.fill.ancestorTagNames(element);
  const seenTagNames = {};
  for (let index = 0; index < tagNames.length; ++index) {
    const tagName = tagNames[index];
    if (tagName in seenTagNames) {
      continue;
    }

    seenTagNames[tagName] = true;
    if (tagName === 'LABEL') {
      inferredLabel = __gCrWeb.fill.inferLabelFromEnclosingLabel(element);
    } else if (tagName === 'DIV') {
      inferredLabel = __gCrWeb.fill.inferLabelFromDivTable(element);
    } else if (tagName === 'TD') {
      inferredLabel = __gCrWeb.fill.inferLabelFromTableColumn(element);
      if (!__gCrWeb.fill.IsLabelValid(inferredLabel)) {
        inferredLabel = __gCrWeb.fill.inferLabelFromTableRow(element);
      }
    } else if (tagName === 'DD') {
      inferredLabel = __gCrWeb.fill.inferLabelFromDefinitionList(element);
    } else if (tagName === 'LI') {
      inferredLabel = __gCrWeb.fill.inferLabelFromListItem(element);
    } else if (tagName === 'FIELDSET') {
      break;
    }

    if (__gCrWeb.fill.IsLabelValid(inferredLabel)) {
      return inferredLabel;
    }
  }
  // If we didn't find a label, check for the value attribute case.
  inferredLabel = __gCrWeb.fill.InferLabelFromValueAttr(element);
  if (__gCrWeb.fill.IsLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  return '';
};
