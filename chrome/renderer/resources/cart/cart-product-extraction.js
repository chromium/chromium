// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var verbose = 0;

// Aliexpress uses 'US $12.34' format in the price.
// Macy's uses "$12.34 to 56.78" format.
var priceCleanupPrefix = 'sale|with offer|only|our price|now|starting at';
var priceCleanupPostfix = '(/(each|set))';
var priceRegexTemplate = '((reg|regular|orig|from|' + priceCleanupPrefix +
    ')\\s+)?' +
    '(\\d+\\s*/\\s*)?(US(D)?\\s*)?' +
    '\\$[\\d.,]+(\\s+(to|-|–)\\s+(\\$)?[\\d.,]+)?' + priceCleanupPostfix + '?';
var priceRegexFull = new RegExp('^' + priceRegexTemplate + '$', 'i');
var priceRegex = new RegExp(priceRegexTemplate, 'i');
var priceCleanupRegex = new RegExp(
    '^((' + priceCleanupPrefix + ')\\s+)|' + priceCleanupPostfix + '$', 'i');
var cartItemHTMLRegex = new RegExp('(cart|basket|bundle)[-_]?item', 'i')
var cartItemTextContentRegex = new RegExp(
    'remove|delete|save for later|move to (favo(u?)rite|list|wish( ?)list)s?',
    'i')
var notCartItemTextContentRegex = new RegExp('move to cart', 'i')

function getLazyLoadingURL(image) {
  // FIXME: some lazy images in Nordstrom and Staples don't have URLs in the
  // DOM.
  // TODO: add more lazy-loading attributes.
  for (const attribute
           of ['data-src', 'data-img-url', 'data-config-src', 'data-echo',
               'data-lazy']) {
    let url = image.getAttribute(attribute);
    if (url == null)
      continue;
    if (url.substr(0, 2) == '//')
      url = 'https:' + url;
    if (url.substr(0, 4) != 'http')
      continue;
    return url;
  }
}

function getLargeImages(root, atLeast, relaxed = false) {
  let candidates = root.querySelectorAll('img');
  if (candidates.length == 0) {
    // Aliexpress
    candidates = root.querySelectorAll('amp-img');
  }
  images = [];
  function shouldStillKeep(image) {
    if (!relaxed)
      return false;
    if (image.getAttribute('aria-hidden') == 'true')
      return true;
    if (getLazyLoadingURL(image) != null)
      return true;
    // For test files on target.com the images aren't preserved for
    // some products so we need to look for the images in the parent
    // picture tag.
    if (image.parentElement.tagName == 'PICTURE')
      return true;
    return false;
  }
  for (const image of candidates) {
    if (verbose > 1)
      console.log('offsetHeight', image, image.offsetHeight);
    if (image.offsetHeight < atLeast) {
      if (!shouldStillKeep(image))
        continue;
    }
    if (window.getComputedStyle(image)['visibility'] == 'hidden')
      continue;
    images.push(image);
  }
  return images;
}

function getVisibleElements(list) {
  visible = [];
  for (const ele of list) {
    if (ele.offsetHeight == 0 || ele.offsetHeight == 0)
      continue;
    visible.push(ele);
  }
  return visible;
}

// Some sites e.g. CraigsList have multiple images per product
function multipleImagesSupported() {
  const hostname = new URL(document.baseURI).hostname;
  // When saving target.com to mhtml, the color selecting images become very
  // large and are picked up. Adding in hostname.endsWith('target.com') is a
  // workaround for this problem. In target we only get one image per product.
  return hostname.endsWith('craigslist.org') || hostname.endsWith('target.com');
}

function extractImage(item) {
  // Sometimes an item contains small icons, which need to be filtered out.
  // TODO: two pass getLargeImages() is probably too slow.
  let images = getLargeImages(item, 40);
  if (images.length == 0) {
    images = getLargeImages(item, 30, true);
  }

  if (images.length == 0) {
    return null;
  }
  if (!multipleImagesSupported()) {
    if (verbose > 0)
      console.assert(
          images.length == 1, 'image extraction error', item, images);
    if (images.length != 1) {
      return null;
    }
  }
  const image = images[0];
  const lazyUrl = getLazyLoadingURL(image);
  if (lazyUrl != null)
    return lazyUrl;

  // If |image| is <amp-img>, image.src won't work.
  const src = image.src || image.getAttribute('src');
  if (verbose > 1)
    console.log('image src', src);
  if (src != null) {
    // data: images are usually placeholders.
    // Even if it's valid, we prefer http(s) URLs.
    if (!src.startsWith('data:')) {
      // Get absolute URL in case it's <amp-img>.
      return (new URL(src, document.location)).href
    }
  }
  let sourceSet = image.getAttribute('data-search-image-source-set');
  if (sourceSet == null && image.parentElement.tagName == 'PICTURE') {
    let sources = image.parentElement.querySelectorAll('source');
    if (sources.length >= 1) {
      sourceSet = getAbsoluteUrlOfSrcSet(sources[0]);
    }
  }
  if (sourceSet == null)
    return null;
  console.assert(sourceSet.includes(' '), 'image extraction error', image);
  // TODO: Pick the one with right pixel density?
  imageUrl = sourceSet.split(' ')[0];
  console.assert(imageUrl.length > 0, 'image extraction error', sourceSet);
  return imageUrl;
}

// Use self assigning trick to get absolute URL
// https://github.com/chromium/dom-distiller/blob/ccfe233400cc214717ccc80973be431ab0e33cf7/java/org/chromium/distiller/DomUtil.java#L438
function getAbsoluteUrlOfSrcSet(image) {
  // preserve src
  const backup = image.src;
  // use self assigning trick
  image.src = image.srcset;
  // clean up and return absolute url
  const ret = image.src;
  image.src = backup;
  return ret;
}

function extractUrl(item) {
  let anchors;
  if (item.tagName == 'A') {
    anchors = [item];
  } else {
    anchors = item.querySelectorAll('a');
  }
  console.assert(anchors.length >= 1, 'url extraction error', item);
  if (anchors.length == 0) {
    return null;
  }
  const filtered = [];
  for (const anchor of anchors) {
    if (anchor.href.match(/\/#$/))
      continue;
    // href="javascript:" would be sanitized when serialized to MHTML.
    if (anchor.href.match(/^javascript:/))
      continue;
    if (anchor.href == '') {
      // For Sears
      let href = anchor.getAttribute('bot-href');
      if (href != null && href.length > 0) {
        // Resolve to absolute URL.
        anchor.href = href;
        href = anchor.href;
        anchor.removeAttribute('href');
        if (href != '')
          return href;
      }
      continue;
    }
    filtered.push(anchor);
    // TODO: This returns the first URL in DOM order.
    //       Use the one with largest area instead?
    return anchor.href;
  }
  if (filtered.length == 0)
    return null;
  return filtered
      .reduce(function(a, b) {
        return a.offsetHeight * a.offsetWidth > b.offsetHeight * b.offsetWidth ?
            a :
            b;
      })
      .href;
}

function isInlineDisplay(element) {
  const display = window.getComputedStyle(element)['display'];
  return display.indexOf('inline') != -1;
}

function childElementCountExcludingInline(element) {
  let count = 0;
  for (const child of element.children) {
    if (isInlineDisplay(child))
      count += 1;
  }
  return count;
}

function hasNonInlineDescendentsInclusive(element) {
  if (!isInlineDisplay(element))
    return true;
  return hasNonInlineDescendents(element);
}

function hasNonInlineDescendents(element) {
  for (const child of element.children) {
    if (hasNonInlineDescendentsInclusive(child))
      return true;
  }
  return false;
}

function hasNonWhiteTextNodes(element) {
  for (const child of element.childNodes) {
    if (child.nodeType != document.TEXT_NODE)
      continue;
    if (child.nodeValue.trim() != '')
      return true;
  }
  return false;
}

// Concat classNames and IDs of ancestors up to |maxDepth|, while not containing
// |excludingElement|.
// If |excludingElement| is already a descendent of |element|, still return the
// className of |element|.
// |maxDepth| include current level, so maxDepth = 1 means just |element|.
// maxDepth >= 3 causes error in Walmart deals if not deducting "price".
function ancestorIdAndClassNames(element, excludingElement, maxDepth = 3) {
  let name = '';
  let depth = 0;
  while (true) {
    name += element.className + element.id;
    element = element.parentElement;
    depth += 1;
    if (depth >= maxDepth)
      break;
    if (!element)
      break;
    if (element.contains(excludingElement))
      break;
  }
  return name;
}

/*
  Returns top-ranked element with the following criteria, with decreasing
  priority:
  - score based on whether ancestorIdAndClassNames contains "title", "price",
  etc.
  - largest area
  - largest font size
  - longest text
 */
function chooseTitle(elementArray) {
  return elementArray.reduce(function(a, b) {
    // Titles are typically 2 characters or more - if one element
    // has less than 2 characters, don't use it.
    const a_len_score = (a.innerText.trim().length >= 2);
    const b_len_score = (b.innerText.trim().length >= 2);
    if (a_len_score != b_len_score) {
      return a_len_score > b_len_score ? a : b;
    }

    const titleRegex = /name|title|truncate|desc|brand/i;
    const negativeRegex = /price|model/i;
    const a_str = ancestorIdAndClassNames(a, b);
    const b_str = ancestorIdAndClassNames(b, a);
    const a_score = (a_str.match(titleRegex) != null) -
        (a_str.match(negativeRegex) != null);
    const b_score = (b_str.match(titleRegex) != null) -
        (b_str.match(negativeRegex) != null);
    if (verbose > 1)
      console.log('className score', a_score, b_score, a_str, b_str, a, b);

    if (a_score != b_score) {
      return a_score > b_score ? a : b;
    }

    // Use getBoundingClientRect() to avoid int rounding error in
    // offsetHeight/Width.
    const a_area =
        a.getBoundingClientRect().width * a.getBoundingClientRect().height;
    const b_area =
        b.getBoundingClientRect().width * b.getBoundingClientRect().height;
    if (verbose > 1)
      console.log(
          'getBoundingClientRect', a.getBoundingClientRect(),
          b.getBoundingClientRect(), a, b);

    if (a_area != b_area) {
      return a_area > b_area ? a : b;
    }

    const a_size = parseFloat(window.getComputedStyle(a)['font-size']);
    const b_size = parseFloat(window.getComputedStyle(b)['font-size']);
    if (verbose > 1)
      console.log('font size', a_size, b_size, a, b);

    if (a_size != b_size) {
      return a_size > b_size ? a : b;
    }

    return a.innerText.length > b.innerText.length ? a : b;
  });
}

function extractTitle(item) {
  const possible_titles =
      item.querySelectorAll('a, span, p, div, h1, h2, h3, h4, h5, strong');
  let titles = [];
  for (const title of possible_titles) {
    if (hasNonInlineDescendents(title) && !hasNonWhiteTextNodes(title)) {
      continue;
    }
    // Too small to be a title.
    if (title.offsetWidth <= 1 || title.offsetHeight <= 1)
      continue;
    if (title.innerText.trim() == '')
      continue;
    if (title.innerText.trim().toLowerCase() == 'sponsored')
      continue;
    if (title.childElementCount > 0) {
      if (title.textContent.trim() ==
              title.lastElementChild.textContent.trim() ||
          title.textContent.trim() ==
              title.firstElementChild.textContent.trim()) {
        continue;
      }
    }
    // Aliexpress has many items without title. Without the following filter,
    // the title would be the price.
    // if (title.innerText.trim().match(priceRegexFull)) continue;
    titles.push(title);
  }
  if (titles.length > 1) {
    if (verbose > 1)
      console.log('all generic titles', item, titles);
    titles = [chooseTitle(titles)];
  }

  if (verbose > 0)
    console.log('titles', item, titles);
  console.assert(titles.length == 1, 'titles extraction error', item, titles);
  if (titles.length != 1)
    return null;
  title = titles[0].innerText.trim();
  return title;
}

function adjustBeautifiedCents(priceElement) {
  const text = priceElement.innerText.trim().replace(/\/(each|set)$/i, '');
  let cents;
  const children = priceElement.children;
  for (let i = children.length - 1; i >= 0; i--) {
    const t = children[i].innerText.trim();
    if (t == '')
      continue;
    if (t.indexOf('/') != -1)
      continue;
    cents = t;
    break;
  }
  if (cents == null)
    return null;
  if (verbose > 0)
    console.log('cents', cents, priceElement);
  if (cents.length == 2 && cents == text.slice(-cents.length) &&
      text.slice(-3, -2).match(/\d/)) {
    return text.substr(0, text.length - cents.length) + '.' + cents;
  }
}

function anyLineThroughInAncentry(element, maxDepth = 2) {
  let depth = 0;
  while (element != null && element.tagName != 'BODY') {
    if (window.getComputedStyle(element)['text-decoration'].indexOf(
            'line-through') != -1)
      return true;
    element = element.parentElement;
    depth += 1;
    if (depth >= maxDepth)
      break;
  }
  return false;
}

function forgivingParseFloat(str) {
  return parseFloat(str.replace(priceCleanupRegex, '').replace(/^[$]*/, ''));
}

function choosePrice(priceArray) {
  if (priceArray.length == 0)
    return null;
  return priceArray
      .reduce(function(a, b) {
        // Positive tags
        for (const pattern of ['with offer', 'sale', 'now']) {
          const a_val = a.toLowerCase().indexOf(pattern) != -1;
          const b_val = b.toLowerCase().indexOf(pattern) != -1;
          if (a_val != b_val) {
            return a_val > b_val ? a : b;
          }
        }
        // Negative tags
        for (const pattern of ['/set', '/each']) {
          const a_val = a.toLowerCase().indexOf(pattern) != -1;
          const b_val = b.toLowerCase().indexOf(pattern) != -1;
          if (a_val != b_val) {
            return a_val < b_val ? a : b;
          }
        }
        // Guess the smallest numerical value.
        // The tags like "now" don't always fall inside element boundary.
        // See Nordstrom/homepage-eager.mhtml.
        return forgivingParseFloat(a) > forgivingParseFloat(b) ? b : a;
      })
      .replace(priceCleanupRegex, '');
}

function extractPrice(item) {
  // Etsy mobile
  const prices = item.querySelectorAll(`
      .currency-value
  `);
  if (prices.length == 1) {
    let ans = prices[0].textContent.trim();
    if (ans.match(/^\d/))
      ans = '$' + ans;  // for Etsy
    if (ans != '')
      return ans;
  }
  // Generic heuristic to search for price elements.
  let captured_prices = [];
  for (const price of item.querySelectorAll('span, b, p, div, h3')) {
    const candidate = price.innerText.trim();
    if (!candidate.match(priceRegexFull))
      continue;
    if (verbose > 1)
      console.log('price candidate', candidate, price);
    if (price.childElementCount > 0) {
      // Avoid matching the parent element of the real price element.
      // Otherwise adjustBeautifiedCents would break.
      if (price.innerText.trim() == price.lastElementChild.innerText.trim() ||
          price.innerText.trim() == price.firstElementChild.innerText.trim()) {
        // If the wanted child is not scanned, change the querySelectorAll
        // string.
        if (verbose > 1)
          console.log('skip redundant parent', price);
        continue;
      }
    }
    // TODO: check child elements recursively.
    if (anyLineThroughInAncentry(price)) {
      if (verbose > 1)
        console.log('line-through', price);
      continue;
    }
    // for Amazon and HomeDepot
    if (candidate.indexOf('.') == -1 && price.lastElementChild != null) {
      const adjusted = adjustBeautifiedCents(price);
      if (adjusted != null)
        return adjusted;
    }
    captured_prices.push(candidate);
  }
  if (verbose > 0)
    console.log('captured_prices', captured_prices);
  return choosePrice(captured_prices);
}

function extractItem(item) {
  imageUrl = extractImage(item);
  if (imageUrl == null) {
    if (verbose > 0)
      console.warn('no images found', item);
    return null;
  }
  url = extractUrl(item);
  // Some items in Sears and Staples only have ng-click or onclick handlers,
  // so it's impossible to extract URL.
  if (url == null) {
    if (verbose > 0)
      console.warn('no url found', item);
    return null;
  }
  title = extractTitle(item);
  if (title == null) {
    if (verbose > 0)
      console.warn('no title found', item);
    return null;
  }
  price = extractPrice(item);
  // eBay "You may also like" and "Guides" are not product items.
  // Not having price is one hint.
  // FIXME: "Also viewed" items in Gap doesn't have prices.
  if (price == null) {
    if (verbose > 0)
      console.warn('no price found', item);
    return null;
  }
  return {'url': url, 'imageUrl': imageUrl, 'title': title, 'price': price};
}

function commonAncestor(a, b) {
  while (!a.contains(b)) {
    a = a.parentElement;
  }
  return a;
}

function commonAncestorList(list) {
  return list.reduce(function(a, b) {
    return commonAncestor(a, b);
  });
}

function hasOverlap(target, list) {
  for (const element of list) {
    if (element.contains(target) || target.contains(element)) {
      return true;
    }
  }
  return false;
}

function isCartItem(item) {
  // TODO: Improve the heuristic here to accommodate more formats of cart item.
  if (item.parentElement) {
    // Walmart has 'move to cart' outside of the div.cart-item.
    if (item.parentElement.textContent.toLowerCase().match(
            notCartItemTextContentRegex))
      return false;
  } else {
    if (item.textContent.toLowerCase().match(notCartItemTextContentRegex))
      return false;
  }
  return item.textContent.toLowerCase().match(cartItemTextContentRegex) ||
      item.innerHTML.toLowerCase().match(cartItemHTMLRegex);
}

function extractOneItem(item, extracted_items, processed, output) {
  if (!isCartItem(item))
    return;
  if (verbose > 1)
    console.log('trying', item);
  if (item.childElementCount == 0 && item.parentElement.tagName != 'BODY') {
    // Amazone store page uses overlay <a>.
    item = item.parentElement;
    if (item == null)
      return;
  }
  // scrollHeight could be 0 while getBoundingClientRect().height > 0.
  if (item.getBoundingClientRect().height < 50) {
    if (verbose > 0)
      console.log('too short', item);
    return;
  }
  if (item.scrollHeight > 1000) {
    if (verbose > 0)
      console.log('too tall', item);
    return;
  }
  if (item.getBoundingClientRect().height * item.getBoundingClientRect().width >
      800 * window.innerWidth) {
    if (verbose > 0)
      console.log('too tall', item);
    return;
  }
  if (item.querySelectorAll('img, amp-img').length == 0) {
    if (verbose > 0)
      console.log('no image', item);
    return;
  }
  if (!item.textContent.match(priceRegex)) {
    if (verbose > 0)
      console.log('no price', item);
    return;
  }
  if (hasOverlap(item, extracted_items)) {
    if (verbose > 0)
      console.log('overlap', item);
    return;
  }
  if (processed.has(item))
    return;
  processed.add(item);
  if (verbose > 0)
    console.log('trying', item);
  const extraction = extractItem(item);
  if (extraction != null) {
    output.set(item, extraction);
    extracted_items.push(item);
  }
}

function documentPositionComparator(a, b) {
  if (a === b)
    return 0;
  const position = a.compareDocumentPosition(b);

  if (position & Node.DOCUMENT_POSITION_FOLLOWING ||
      position & Node.DOCUMENT_POSITION_CONTAINED_BY) {
    return -1;
  } else if (
      position & Node.DOCUMENT_POSITION_PRECEDING ||
      position & Node.DOCUMENT_POSITION_CONTAINS) {
    return 1;
  } else {
    return 0;
  }
}

function extractAllItems(root) {
  let items = [];
  // Root element being null could be due to the
  // fact that the cart is emptied, or the cart
  // element has not been loaded yet.
  if (root == null) {
    if (document.readyState == 'complete') {
      return [];
    } else {
      return false;
    }
  }

  // Generic pattern
  const candidates = new Set();
  items = root.querySelectorAll('a');

  const urlMap = new Map();
  for (const item of items) {
    if (!urlMap.has(item.href)) {
      urlMap.set(item.href, new Set());
    }
    urlMap.get(item.href).add(item);
  }

  for (const [key, value] of urlMap) {
    const ancestor = commonAncestorList(Array.from(value));
    if (!candidates.has(ancestor))
      candidates.add(ancestor);
  }
  for (const item of items) {
    candidates.add(item);
  }
  const ancestors = new Set();
  // TODO: optimize this part.
  for (let depth = 0; depth < 8; depth++) {
    for (let item of candidates) {
      for (let i = 0; i < depth; i++) {
        item = item.parentElement;
        if (!item)
          break;
      }
      if (item)
        ancestors.add(item);
    }
  }
  items = Array.from(ancestors);

  if (verbose > 0)
    console.log(items);
  const outputMap = new Map();
  const processed = new Set();
  const extracted_items = [];
  for (const item of items) {
    extractOneItem(item, extracted_items, processed, outputMap);
  }
  const keysInDocOrder =
      Array.from(outputMap.keys()).sort(documentPositionComparator);
  const output = [];
  for (const key of keysInDocOrder) {
    output.push(outputMap.get(key));
  }
  return output;
}

extracted_results = extractAllItems(document);
