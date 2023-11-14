// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';

declare type FormControlElement
    = HTMLInputElement|HTMLTextAreaElement|HTMLSelectElement;

/**
 * Shared function for InferLabelFromPrevious() and InferLabelFromNext().
 *
 * It is based on the logic in
 *     string16 InferLabelFromSibling(const WebFormControlElement& element,
 *                                    bool forward)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @param forward whether to search for the next or previous element.
 * @return The label of element or an empty string if there is no
 *                  sibling or no label.
 */
function inferLabelFromSibling(
    element: FormControlElement | null, forward: boolean): string {
  let inferredLabel = '';
  let sibling: Node | null = element;
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
    if (nodeType === Node.TEXT_NODE || gCrWeb.fill.hasTagName(sibling, 'b') ||
        gCrWeb.fill.hasTagName(sibling, 'strong') ||
        gCrWeb.fill.hasTagName(sibling, 'span') ||
        gCrWeb.fill.hasTagName(sibling, 'font')) {
      const value = inferenceUtil.findChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      const addSpace = nodeType === Node.TEXT_NODE && value.length === 0;
      if (forward) {
        inferredLabel = gCrWeb.fill.combineAndCollapseWhitespace(
            inferredLabel, value, addSpace);
      } else {
        inferredLabel = gCrWeb.fill.combineAndCollapseWhitespace(
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
    if (gCrWeb.fill.hasTagName(sibling, 'img') ||
        gCrWeb.fill.hasTagName(sibling, 'br')) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    if (gCrWeb.fill.hasTagName(sibling, 'p') ||
        gCrWeb.fill.hasTagName(sibling, 'label')) {
      inferredLabel = inferenceUtil.findChildText(sibling);
    }
    break;
  }
  return inferredLabel.trim();
}

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
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromPrevious = function(
    element: FormControlElement): string {
  return inferLabelFromSibling(element, false);
};

/**
 * Same as InferLabelFromPrevious(), but in the other direction.
 * Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
 *
 * It is based on the logic in
 *     string16 InferLabelFromNext(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
function inferLabelFromNext(element: FormControlElement): string {
  return inferLabelFromSibling(element, true);
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the placeholder attribute.
 *
 * It is based on the logic in
 *     string16 InferLabelFromPlaceholder(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
function inferLabelFromPlaceholder(element: FormControlElement): string {
  if (!element) {
    return '';
  }

  if ('placeholder' in element) {
    return element.placeholder;
  }

  return element.getAttribute('placeholder') || '';
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the value attribute when it is present and user has not typed in (if
 * element's value attribute is same as the element's value).
 *
 * It is based on the logic in
 *     string16 InferLabelFromValueAttr(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
function inferLabelFromValueAttr(element: FormControlElement): string {
  if (!element || !element.value || !element.hasAttribute('value') ||
      element.value !== element.getAttribute('value')) {
    return '';
  }

  return element.value;
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * enclosing list item, e.g.
 *     <li>Some Text<input ...><input ...><input ...></li>
 *
 * It is based on the logic in
 *     string16 InferLabelFromListItem(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromListItem = function(
    element: FormControlElement): string {
  if (!element) {
    return '';
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !gCrWeb.fill.hasTagName(parentNode, 'li')) {
    parentNode = parentNode.parentNode;
  }

  if (parentNode && gCrWeb.fill.hasTagName(parentNode, 'li')) {
    return inferenceUtil.findChildText(parentNode);
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
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromTableColumn = function(
    element: FormControlElement): string {
  if (!element) {
    return '';
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !gCrWeb.fill.hasTagName(parentNode, 'td')) {
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
    if (gCrWeb.fill.hasTagName(previous, 'td') ||
        gCrWeb.fill.hasTagName(previous, 'th')) {
      inferredLabel = inferenceUtil.findChildText(previous);
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
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromTableRow = function(
    element: FormControlElement): string {
  if (!element) {
    return '';
  }

  let parentCell = element.parentNode;
  while (parentCell) {
    if (parentCell.nodeType === Node.ELEMENT_NODE &&
        gCrWeb.fill.hasTagName(parentCell, 'td')) {
      break;
    }
    parentCell = parentCell.parentNode;
  }

  const cell: HTMLTableCellElement | null = parentCell as HTMLTableCellElement;
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
        gCrWeb.fill.hasTagName(cellIterator, 'td')) {
      cellPosition += (cellIterator as HTMLTableCellElement).colSpan;
    }
    cellIterator = cellIterator.previousSibling;
  }

  // Count cells to the right.
  cellIterator = cell.nextSibling;
  while (cellIterator) {
    if (cellIterator.nodeType === Node.ELEMENT_NODE &&
        gCrWeb.fill.hasTagName(cellIterator, 'td')) {
      cellCount += (cellIterator as HTMLTableCellElement).colSpan;
    }
    cellIterator = cellIterator.nextSibling;
  }

  // Combine left + right.
  cellCount += cellPosition;
  cellPositionEnd += cellPosition;

  // Find the current row.
  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !gCrWeb.fill.hasTagName(parentNode, 'tr')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return '';
  }

  // Now find the previous row.
  let rowIt = parentNode.previousSibling;
  while (rowIt) {
    if (rowIt.nodeType === Node.ELEMENT_NODE &&
        gCrWeb.fill.hasTagName(parentNode, 'tr')) {
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
        if (gCrWeb.fill.hasTagName(prevRowIt, 'td') ||
            gCrWeb.fill.hasTagName(prevRowIt, 'th')) {
          const span = (prevRowIt as HTMLTableCellElement).colSpan;
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
      const inferredLabel = inferenceUtil.findChildText(matchingCell);
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
    if (gCrWeb.fill.hasTagName(previous, 'tr')) {
      inferredLabel = inferenceUtil.findChildText(previous);
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
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromEnclosingLabel = function(
    element: FormControlElement): string {
  if (!element) {
    return '';
  }
  let node = element.parentNode;
  while (node && !gCrWeb.fill.hasTagName(node, 'label')) {
    node = node.parentNode;
  }
  if (node) {
    return inferenceUtil.findChildText(node);
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
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromDivTable = function(
    element: FormControlElement): string {
  if (!element) {
    return '';
  }

  let node: ParentNode | ChildNode | null = element.parentNode;
  let lookingForParent = true;
  const divsToSkip: Node[] = [];

  // Search the sibling and parent <div>s until we find a candidate label.
  let inferredLabel = '';
  while (inferredLabel.length === 0 && node) {
    if (gCrWeb.fill.hasTagName(node, 'div')) {
      if (lookingForParent) {
        inferredLabel =
            inferenceUtil.findChildTextWithIgnoreList(node, divsToSkip);
      } else {
        inferredLabel = inferenceUtil.findChildText(node);
      }
      // Avoid sibling DIVs that contain autofillable fields.
      if (!lookingForParent &&inferredLabel.length > 0) {
        const resultElement = (node as HTMLDivElement)
            .querySelector('input, select, textarea');
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
      if (gCrWeb.fill.hasTagName(node, 'label') &&
          !(node as HTMLLabelElement).control) {
        inferredLabel = inferenceUtil.findChildText(node);
      } else if (node.nodeType === Node.TEXT_NODE) {
        inferredLabel = inferenceUtil.nodeValue(node).trim();
      }
    } else if (inferenceUtil.isTraversableContainerElement(node)) {
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
 * @param element An element to examine.
 * @return The label of element.
 */
gCrWeb.fill.inferLabelFromDefinitionList = function(
    element: FormControlElement): string {
  if (!element) {
    return '';
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !gCrWeb.fill.hasTagName(parentNode, 'dd')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode || !gCrWeb.fill.hasTagName(parentNode, 'dd')) {
    return '';
  }

  // Skip by any intervening text nodes.
  let previous = parentNode.previousSibling;
  while (previous && previous.nodeType === Node.TEXT_NODE) {
    previous = previous.previousSibling;
  }

  if (!previous || !gCrWeb.fill.hasTagName(previous, 'dt')) {
    return '';
  }

  return inferenceUtil.findChildText(previous);
};

/**
 * Infers corresponding label for |element| from surrounding context in the DOM,
 * e.g. the contents of the preceding <p> tag or text element.
 *
 * It is based on the logic in
 *    string16 InferLabelForElement(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The inferred label of element, or '' if none could be found.
 */
gCrWeb.fill.inferLabelForElement = function(
    element: FormControlElement): string {
  let inferredLabel;
  if (gCrWeb.fill.isCheckableElement(element)) {
    inferredLabel = inferLabelFromNext(element);
    if (inferenceUtil.isLabelValid(inferredLabel)) {
      return inferredLabel;
    }
  }

  inferredLabel = gCrWeb.fill.inferLabelFromPrevious(element);
  if (inferenceUtil.isLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  // If we didn't find a label, check for the placeholder case.
  inferredLabel = inferLabelFromPlaceholder(element);
  if (inferenceUtil.isLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  // If we didn't find a placeholder, check for the aria-label case.
  inferredLabel = gCrWeb.fill.getAriaLabel(element);
  if (inferenceUtil.isLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  // For all other searches that involve traversing up the tree, the search
  // order is based on which tag is the closest ancestor to |element|.
  const tagNames = inferenceUtil.ancestorTagNames(element);
  const seenTagNames: {[key: string]: boolean} = {};
  for (let index = 0; index < tagNames.length; ++index) {
    const tagName = tagNames[index]!;
    if (tagName in seenTagNames) {
      continue;
    }

    seenTagNames[tagName] = true;
    if (tagName === 'LABEL') {
      inferredLabel = gCrWeb.fill.inferLabelFromEnclosingLabel(element);
    } else if (tagName === 'DIV') {
      inferredLabel = gCrWeb.fill.inferLabelFromDivTable(element);
    } else if (tagName === 'TD') {
      inferredLabel = gCrWeb.fill.inferLabelFromTableColumn(element);
      if (!inferenceUtil.isLabelValid(inferredLabel)) {
        inferredLabel = gCrWeb.fill.inferLabelFromTableRow(element);
      }
    } else if (tagName === 'DD') {
      inferredLabel = gCrWeb.fill.inferLabelFromDefinitionList(element);
    } else if (tagName === 'LI') {
      inferredLabel = gCrWeb.fill.inferLabelFromListItem(element);
    } else if (tagName === 'FIELDSET') {
      break;
    }

    if (inferenceUtil.isLabelValid(inferredLabel)) {
      return inferredLabel;
    }
  }
  // If we didn't find a label, check for the value attribute case.
  inferredLabel = inferLabelFromValueAttr(element);
  if (inferenceUtil.isLabelValid(inferredLabel)) {
    return inferredLabel;
  }

  return '';
};

export {inferLabelFromNext};
