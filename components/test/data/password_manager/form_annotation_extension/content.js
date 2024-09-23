// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Find one or two CSS classes in the class list |list| that uniquely identify
 * the element in the document |doc|.
 * If the two classes are not enough to uniquely identify the element, chooses
 * two classes that minimize the number of elements (that have these classes).
 *
 * @param {string} list The class list of the element to be uniquely identified.
 * @param {Document} doc The owner document of the element.
 * @return {string} One or two class names joined with a space.
 */
function reduceClassName(list, doc) {
  let minCount = 1000000;
  let minCountClasses = '';
  for (i = 0; i < list.length; i++) {
    const className = list.item(i);
    const count = doc.getElementsByClassName(className).length;
    if (count == 1) {
      return '.' + className;
    }
    if (count < minCount) {
      minCount = count;
      minCountClasses = '.' + className;
    }
  }
  for (i = 0; i < list.length; i++) {
    const className1 = list.item(i);
    for (j = 0; j < list.length; j++) {
      const className2 = list.item(j);
      const count =
          doc.getElementsByClassName(className1 + ' ' + className2).length;
      if (count == 1) {
        return '.' + className1 + '.' + className2;
      }
      if (count < minCount) {
        minCount = count;
        minCountClasses = '.' + className1 + '.' + className2;
      }
    }
  }
  return minCountClasses;
}

/**
 * For the given element |elem|, returns the number of previous siblings that
 * have LI tag.
 *
 * @param {Element} elem The element siblings of which should be counted.
 * @return {number} The number of siblings with LI tag.
 */
function getIndexInChildrenList(elem) {
  let result = 1;
  let sibling = elem.previousSibling;
  while (sibling) {
    if (sibling.tagName == 'LI') {
      result++;
    }
    sibling = sibling.previousSibling;
  }
  return result;
}

/**
 * Returns compact CSS selector that uniquely identifies |elem|.
 * MOST IMPORTANT part in the extension. By a number of heuristics, it creates
 * a CSS selector that would be persistent to web page changes (e.g. full path
 * to the element |elem| is a bad idea), but allows to find element |elem|.
 * TODO(crbug.com/41255533): sometimes it fails to build unique selector. Fix it.
 *
 * @param {Element} elem The element which CSS selector should be created.
 * @return {string} CSS selector of the element.
 */
function getSmartSelector(elem) {
  const doc = elem.ownerDocument;
  let result = elem.tagName;

  if (elem.id) {
    result += '[id=\'' + elem.id + '\']';
  }  // Works for IDs started with a digit.
  if (elem.name) {
    result += '[name=\'' + elem.name + '\']';
  }
  if (elem.tagName == 'INPUT' && elem.type) {
    result += '[type=\'' + elem.type + '\']';
  }
  if (elem.classList.length > 0) {
    result += reduceClassName(elem.classList, doc);
  }
  if (elem.tagName == 'LI') {
    result += ':nth-child(' + getIndexInChildrenList(elem) + ')';
  }

  // If failed to build a unique selector for |elem|, try to add the parent CSS
  // selector.
  if (doc.querySelectorAll(result).length != 1) {
    if (elem.parentElement) {
      const parentSelector = getSmartSelector(elem.parentElement);
      if (parentSelector) {
        return parentSelector + ' > ' + result;
      }
    }
    console.error('failed to build unique css selector ' + result + ': ' +
        doc.querySelectorAll(result));
    return '';
  } else {
    return result;
  }
}

/**
 * Returns CSS selectors of parent frames.
 * Doesn't work for cross-domain iframes. TODO(crbug.com/41255536): Chrome
 * extensions should be able to process cross-domain stuff. Fix it.
 *
 * @param {Element} elem The element which parent frames should be returned.
 * @return {Array} The array of CSS selectors of parent frames.
 */
function getFrames(elem) {
  frames = [];
  while (elem.ownerDocument.defaultView != top) {
    const frameElement = elem.ownerDocument.defaultView.frameElement;
    if (!frameElement) {
      console.error('frameElement is null. Unable to fetch data about iframes');
      break;
    }
    const iframeSelector = getSmartSelector(frameElement);
    frames.unshift(iframeSelector);
    elem = elem.ownerDocument.defaultView.frameElement;
  }
  return frames;
}

/**
 * Returns true if |element| is probably a clickable element.
 *
 * @param {Element} element The element to be checked.
 * @return {boolean} True if the element is probably clickable.
 */
function isClickableElementOrInput(element) {
  return (element.tagName == 'INPUT' || element.tagName == 'A' ||
      element.tagName == 'BUTTON' || element.tagName == 'SUBMIT' ||
      element.getAttribute('href'));
}

/**
 * Returns |element|, if |element| is clickable element. Othrewise, returns
 * clickable children or parent of the given element |element|.
 * Font element might consume a user click, but Chrome Driver will be unable to
 * click on the font element, so find really clickable element to perform click.
 *
 * @param {Element} element The element where a clickable tag should be find.
 * @return {Element} The clicable element.
 */
function fixElementSelection(element) {
  if (isClickableElementOrInput(element)) {
    return element;
  }
  const clickableChildren = element.querySelectorAll(
      ':scope input, :scope a, :scope button, :scope submit, :scope [href]');
  if (clickableChildren.length > 0) {
    return clickableChildren[0];
  }
  let parent = element;
  for (let i = 0; i < 5; i++) {
    parent = parent.parentElement;
    if (!parent) {
      break;
    }
    if (isClickableElementOrInput(parent)) {
      return parent;
    }
  }
  return element;
}

/**
 * Check if it is possible to fetch the owner document of |elem| and find |elem|
 * there.
 * Sometimes getting to the form require a number of steps. So, keeping just URL
 * of the last step is not enough. This function checks if it is possible to
 * start with the |elem.ownerDocument.URL|.
 *
 * @param {Element} elem The element to be checked.
 * @param {string} selector The CSS selector of |elem|.
 * @return {boolean} True, if it is possible find an element with CSS selector
 *    |selector| on |elem.ownerDocument.URL|.
 */
function couldBeFirstStepOfScript(elem, selector) {
  try {
    const xmlHttp = new XMLHttpRequest();
    xmlHttp.open('GET', elem.ownerDocument.URL,
                 false /* false for synchronous request */);
    xmlHttp.send();
    const wrapper = document.createElement('html');
    wrapper.innerHTML = xmlHttp.responseText;
    const e = wrapper.querySelector(selector);
    return e && (e.offsetWidth * e.offsetHeight > 0);
  } catch (err) {
    return false;
  }
}

/**
 * Add click listener.
 */
document.addEventListener('click', function(event) {
  const element = fixElementSelection(event.target);
  const url = element.ownerDocument.URL;
  const isPwdField = (element.tagName == 'INPUT') && (element.type == 'password');
  const selector = getSmartSelector(element);
  const frames = getFrames(element);
  const classifierOutcome = element.hasAttribute('pm_debug_pwd_creation_field');
  const couldBeFirst = couldBeFirstStepOfScript(element, selector);
  chrome.runtime.sendMessage(
    {isPwdField: isPwdField, selector: selector, url: url, frames: frames,
     couldBeFirst: couldBeFirst, classifierOutcome: classifierOutcome},
    function(response) {});
}, true);
