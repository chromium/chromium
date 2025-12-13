// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FormControlElement} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import * as inferenceUtil from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import {ancestorTagNames, buildInferredLabelIfValid, findChildText, findChildTextWithIgnoreList, isTraversableContainerElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import * as fillUtil from '//components/autofill/ios/form_util/resources/fill_util.js';

/**
 * Shared function for InferLabelFromPrevious() and InferLabelFromNext().
 *
 * It is based on the logic in InferLabelFromSibling() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @param forward whether to search for the next or previous element.
 * @return The label of element or an empty string if there is no
 *                  sibling or no label.
 */
function inferLabelFromSibling(
    element: FormControlElement|null,
    forward: boolean): inferenceUtil.InferredLabel|null {
  let inferredLabel = '';
  let sibling: Node | null = element;
  if (!sibling) {
    return null;
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
    // TODO(crbug.com/454044167): Cleanup autofill TS type casting.
    if (nodeType === Node.TEXT_NODE ||
        inferenceUtil.hasTagName(sibling as Element, 'b') ||
        inferenceUtil.hasTagName(sibling as Element, 'strong') ||
        inferenceUtil.hasTagName(sibling as Element, 'span') ||
        inferenceUtil.hasTagName(sibling as Element, 'font')) {
      const value = findChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      const addSpace = nodeType === Node.TEXT_NODE && value.length === 0;
      if (forward) {
        inferredLabel = inferenceUtil.combineAndCollapseWhitespace(
            inferredLabel, value, addSpace);
      } else {
        inferredLabel = inferenceUtil.combineAndCollapseWhitespace(
            value, inferredLabel, addSpace);
      }
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    const r = buildInferredLabelIfValid(inferredLabel);
    if (r) {
      return r;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (inferenceUtil.hasTagName(sibling as Element, 'img') ||
        inferenceUtil.hasTagName(sibling as Element, 'br')) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    if (inferenceUtil.hasTagName(sibling as Element, 'p') ||
        inferenceUtil.hasTagName(sibling as Element, 'label')) {
      return buildInferredLabelIfValid(findChildText(sibling));
    }
    break;
  }
  return buildInferredLabelIfValid(inferredLabel);
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
 * It is based on the logic in InferLabelFromPrevious() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
export function inferLabelFromPrevious(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  return inferLabelFromSibling(element, false);
}

/**
 * Same as InferLabelFromPrevious(), but in the other direction.
 * Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
 *
 * It is based on the logic in InferLabelFromNext() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
export function inferLabelFromNext(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  return inferLabelFromSibling(element, true);
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the placeholder attribute.
 *
 * It is based on the logic in InferLabelFromPlaceholder() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
function inferLabelFromPlaceholder(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  if ('placeholder' in element) {
    return buildInferredLabelIfValid(element.placeholder);
  }

  return buildInferredLabelIfValid(element.getAttribute('placeholder') || '');
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the aria-label attribute.
 *
 * It is based on the logic in InferLabelFromAriaLabel() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
function inferLabelFromAriaLabel(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  return buildInferredLabelIfValid(fillUtil.getAriaLabel(element));
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * the value attribute when it is present and user has not typed in (if
 * element's value attribute is same as the element's value).
 *
 * It is based on the logic in InferLabelFromValueAttr() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
function inferLabelFromValueAttr(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element || !element.value || !element.hasAttribute('value') ||
      element.value !== element.getAttribute('value')) {
    return null;
  }

  return buildInferredLabelIfValid(element.value);
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * enclosing list item, e.g.
 *     <li>Some Text<input ...><input ...><input ...></li>
 *
 * It is based on the logic in InferLabelFromListItem() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
// TODO(crbug.com/454044167): Cleanup autofill TS type casting.
export function inferLabelFromListItem(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !inferenceUtil.hasTagName(parentNode as Element, 'li')) {
    parentNode = parentNode.parentNode;
  }

  if (parentNode && inferenceUtil.hasTagName(parentNode as Element, 'li')) {
    return buildInferredLabelIfValid(findChildText(parentNode));
  }

  return null;
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * surrounding table structure,
 * e.g. <tr><td>Some Text</td><td><input ...></td></tr>
 * or   <tr><th>Some Text</th><td><input ...></td></tr>
 * or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
 * or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
 *
 * It is based on the logic in InferLabelFromTableColumn() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
// TODO(crbug.com/454044167): Cleanup autofill TS type casting.
export function inferLabelFromTableColumn(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !inferenceUtil.hasTagName(parentNode as Element, 'td')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return null;
  }

  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  let r: inferenceUtil.InferredLabel|null = null;
  let previous = parentNode.previousSibling;
  while (!r && previous) {
    if (inferenceUtil.hasTagName(previous as Element, 'td') ||
        inferenceUtil.hasTagName(previous as Element, 'th')) {
      r = buildInferredLabelIfValid(findChildText(previous));
    }
    previous = previous.previousSibling;
  }

  return r;
}

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
 * It is based on the logic in InferLabelFromTableRow() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
// TODO(crbug.com/454044167): Cleanup autofill TS type casting.
export function inferLabelFromTableRow(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  let parentCell = element.parentNode;
  while (parentCell) {
    if (parentCell.nodeType === Node.ELEMENT_NODE &&
        inferenceUtil.hasTagName(parentCell as Element, 'td')) {
      break;
    }
    parentCell = parentCell.parentNode;
  }

  const cell: HTMLTableCellElement | null = parentCell as HTMLTableCellElement;
  // Not in a cell - bail out.
  if (!cell) {
    return null;
  }

  // Count the cell holding |element|.
  let cellCount = cell.colSpan;
  let cellPosition = 0;
  let cellPositionEnd = cellCount - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  let cellIterator = cell.previousSibling;
  while (cellIterator) {
    if (cellIterator.nodeType === Node.ELEMENT_NODE &&
        inferenceUtil.hasTagName(cellIterator as Element, 'td')) {
      cellPosition += (cellIterator as HTMLTableCellElement).colSpan;
    }
    cellIterator = cellIterator.previousSibling;
  }

  // Count cells to the right.
  cellIterator = cell.nextSibling;
  while (cellIterator) {
    if (cellIterator.nodeType === Node.ELEMENT_NODE &&
        inferenceUtil.hasTagName(cellIterator as Element, 'td')) {
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
         !inferenceUtil.hasTagName(parentNode as Element, 'tr')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode) {
    return null;
  }

  // Now find the previous row.
  let rowIt = parentNode.previousSibling;
  while (rowIt) {
    if (rowIt.nodeType === Node.ELEMENT_NODE &&
        inferenceUtil.hasTagName(parentNode as Element, 'tr')) {
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
        if (inferenceUtil.hasTagName(prevRowIt as Element, 'td') ||
            inferenceUtil.hasTagName(prevRowIt as Element, 'th')) {
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
      const r = buildInferredLabelIfValid(findChildText(matchingCell));
      if (r) {
        return r;
      }
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  let r: inferenceUtil.InferredLabel|null = null;
  let previous = parentNode.previousSibling;
  while (!r && previous) {
    if (inferenceUtil.hasTagName(previous as Element, 'tr')) {
      r = buildInferredLabelIfValid(findChildText(previous));
    }
    previous = previous.previousSibling;
  }
  return r;
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * an enclosing label.
 * e.g. <label>Some Text<span><input ...></span></label>
 *
 * It is based on the logic in InferLabelFromEnclosingLabel() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
export function inferLabelFromEnclosingLabel(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }
  let node = element.parentNode;
  while (node && !inferenceUtil.hasTagName(node as Element, 'label')) {
    node = node.parentNode;
  }
  if (node) {
    return buildInferredLabelIfValid(findChildText(node));
  }
  return null;
}

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
 * It is based on the logic in InferLabelFromDivTable() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */
// TODO(crbug.com/454044167): Cleanup autofill TS type casting.

export function inferLabelFromDivTable(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  let node: ParentNode | ChildNode | null = element.parentNode;
  let lookingForParent = true;
  const divsToSkip: Node[] = [];

  // Search the sibling and parent <div>s until we find a candidate label.
  let r: inferenceUtil.InferredLabel|null = null;
  while (!r && node) {
    if (inferenceUtil.hasTagName(node as Element, 'div')) {
      r = buildInferredLabelIfValid(
          lookingForParent
              ? findChildTextWithIgnoreList(node, divsToSkip)
              : findChildText(node));
      // Avoid sibling DIVs that contain autofillable fields.
      if (!lookingForParent && r) {
        const resultElement = (node as HTMLDivElement)
            .querySelector('input, select, textarea');
        if (resultElement) {
          r = null;
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
      if (node.nodeType === Node.TEXT_NODE ||
          (inferenceUtil.hasTagName(node as Element, 'label') &&
           !(node as HTMLLabelElement).control)) {
        r = buildInferredLabelIfValid(findChildText(node));
      }
    } else if (isTraversableContainerElement(node)) {
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

  return r;
}

/**
 * Helper for |InferLabelForElement()| that infers a label, if possible, from
 * a surrounding definition list,
 * e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
 * e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
 *
 * It is based on the logic in InferLabelFromDefinitionList() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The label of element.
 */

export function inferLabelFromDefinitionList(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  if (!element) {
    return null;
  }

  let parentNode = element.parentNode;
  while (parentNode && parentNode.nodeType === Node.ELEMENT_NODE &&
         !inferenceUtil.hasTagName(parentNode as Element, 'dd')) {
    parentNode = parentNode.parentNode;
  }

  if (!parentNode || !inferenceUtil.hasTagName(parentNode as Element, 'dd')) {
    return null;
  }

  // Skip by any intervening text nodes.
  let previous = parentNode.previousSibling;
  while (previous && previous.nodeType === Node.TEXT_NODE) {
    previous = previous.previousSibling;
  }

  if (!previous || !inferenceUtil.hasTagName(previous as Element, 'dt')) {
    return null;
  }

  return buildInferredLabelIfValid(findChildText(previous));
}

/**
 * Infers corresponding label for |element| from surrounding context in the DOM,
 * e.g. the contents of the preceding <p> tag or text element.
 *
 * It is based on the logic in InferLabelForElement() in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return The inferred label of element, or '' if none could be found.
 */
export function inferLabelForElement(element: FormControlElement):
    inferenceUtil.InferredLabel|null {
  let r: inferenceUtil.InferredLabel|null = null;
  if (inferenceUtil.isCheckableElement(element)) {
    r = inferLabelFromNext(element);
    if (r) {
      return r;
    }
  }

  r = inferLabelFromPrevious(element);
  if (r) {
    return r;
  }

  // If we didn't find a label, check for the placeholder case.
  r = inferLabelFromPlaceholder(element);
  if (r) {
    return r;
  }

  // If we didn't find a placeholder, check for the aria-label case.
  r = inferLabelFromAriaLabel(element);
  if (r) {
    return r;
  }

  // For all other searches that involve traversing up the tree, the search
  // order is based on which tag is the closest ancestor to |element|.
  // TODO(crbug.com/337179781): Match with C++ InferLabelFromAncestors().
  const tagNames = ancestorTagNames(element);
  const seenTagNames: {[key: string]: boolean} = {};
  for (let index = 0; index < tagNames.length; ++index) {
    const tagName = tagNames[index]!;
    if (tagName in seenTagNames) {
      continue;
    }

    seenTagNames[tagName] = true;
    if (tagName === 'LABEL') {
      r = inferLabelFromEnclosingLabel(element);
    } else if (tagName === 'DIV') {
      r = inferLabelFromDivTable(element);
    } else if (tagName === 'TD') {
      r = inferLabelFromTableColumn(element);
      if (!r) {
        r = inferLabelFromTableRow(element);
      }
    } else if (tagName === 'DD') {
      r = inferLabelFromDefinitionList(element);
    } else if (tagName === 'LI') {
      r = inferLabelFromListItem(element);
    } else if (tagName === 'FIELDSET') {
      break;
    }

    if (r) {
      return r;
    }
  }
  // If we didn't find a label, check for the value attribute case.
  return inferLabelFromValueAttr(element);
}
