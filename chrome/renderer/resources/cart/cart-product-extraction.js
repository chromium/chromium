// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var verbose = 0;

// Aliexpress uses 'US $12.34' format in the price.
// Macy's uses "$12.34 to 56.78" format.
var priceCleanupPrefix = 'total price|sale price|price|sale|' +
    'with offer|only|our price|now|starting at';
var priceCleanupPostfix = '(/(each|set))';
var priceRegexTemplate = '((reg|regular|orig|from|' + priceCleanupPrefix +
    ')\\s+)?' +
    '(\\d+\\s*/\\s*)?(US(D)?\\s*)?' +
    '\\$\\s*[\\d.,]+(\\s+(to|-|–)\\s+(\\$)?[\\d.,]+)?' +
    priceCleanupPostfix + '?';
var priceRegexFull = new RegExp('^' + priceRegexTemplate + '( ea)?$', 'i');
var priceRegex = new RegExp(priceRegexTemplate, 'i');
var priceCleanupRegex = new RegExp(
    '^((' + priceCleanupPrefix + ')\\s+)|' + priceCleanupPostfix + '$', 'i');
var cartItemHTMLRegex = new RegExp(
    '(cart|basket|bundle)[-_]?((\\w+)[-_])?(item|product)', 'i');
var cartItemTextRegex = new RegExp(
    'remove|delete|save for later|move to (favo(u?)rite|list|wish( ?)list)s?',
    'i');
var cartItemQtyRegex = new RegExp('qty', 'i');
var moveToCartTextRegex = new RegExp('move to (cart|bag)', 'i');
var addToCartTextRegex = new RegExp('add to cart', 'i');
var cartPriceTextRegex = new RegExp('((estimated (sales )?)|(sales ))tax', 'i');
var minicartHTMLRegex = new RegExp('mini-cart-product', 'i');
var productIdHTMLRegex = new RegExp('<a href="#modal-(\\w+)', 'i');
var productIdURLRegex = new RegExp(
    '((\\w+)-\\d+-medium)|(images.cymax.com/Images/\\d+/(\\w+)-)', 'i');
var saveForLaterRegex = new RegExp('save for later', 'i');

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
  if (candidates.length == 0) {
    // Google store
    candidates = root.querySelectorAll('.bg-img');
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
  return hostname.endsWith('craigslist.org') || hostname.endsWith('target.com')
      || hostname.endsWith('zazzle.com')
      || hostname.endsWith("ashleyfurniture.com")
      || hostname.endsWith("chewy.com");
}

function extractImage(item) {
  const hostname = new URL(document.baseURI).hostname;
  // Some merchant sites have product images as background of a div element.
  // Below logic handles them separately.
  if (hostname.endsWith("americastire.com")
    || hostname.endsWith("discounttire.com")) {
    const image = item.querySelector(".product-image__image-block");
    if (image == null) {
      return null;
    }
    return extractImageUrl(image);
  }
  if (hostname.endsWith("discounttiredirect.com")) {
    const image = item.querySelector(".cart-item__product-image");
    if (image == null) {
      return null;
    }
    return extractImageUrl(image);
  }
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
  if (!document.URL.includes("chewy.com")) {
    images = images.slice(0, 1);
  }
  for (const image of images) {
    const currentUrl = extractImageUrl(image);
    if (currentUrl !== null) return currentUrl;
  }
  return null;
}

function extractImageUrl(image) {
  const lazyUrl = getLazyLoadingURL(image);
  if (lazyUrl != null)
    return lazyUrl;

  // Special handling for Google store, America's Tire and Discount
  // Tire Direct.
  if (image.className === "bg-img"
    || image.className.includes("product-image__image-block")
    || image.className.includes("cart-item__product-image")) {
    if (image.style.backgroundImage == undefined) {
      return null;
    }
    const matches = image.style.backgroundImage.match('[\"\'](.*)[\"\']');
    if (matches === null) {
      return null;
    } else {
      return matches[1];
    }
  }
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
  // Some sites doesn't use <a> tag or explicitly state href. E.g. ae.com
  // shows side panel after clicking on each item instead of directing to
  // product page, and some sites might trigger JS to initiate navigation
  // instead of <a>.
  if (document.URL.includes("ae.com")
      || document.URL.includes("kiehls.com")
      || document.URL.includes("discounttiredirect.com")
      || document.URL.includes("shutterfly.com")
      || document.URL.includes("bkstr.com")) {
    return "";
  }
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
  const hostname = new URL(document.baseURI).hostname;
  // shein.com shows price by one element per digit and it's challenging
  // to decide based on textContent.
  if (hostname.endsWith("shein.com")) {
    return "";
  }
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
  for (const price of item.querySelectorAll(
    'span, b, p, div, h3, td, li, em, strong, ins')) {
    let candidate = price.innerText.trim();
    if (hostname.endsWith("urbanoutfitters.com") ||
        hostname.endsWith("freepeople.com")) {
      priceParts = candidate.split("\n");
      if (priceParts.length >= 2){
        candidate = priceParts[1];
      }
    } else if (hostname.endsWith("thecompanystore.com") ||
        hostname.endsWith("childrensplace.com") ||
        hostname.endsWith("chewy.com")) {
      candidate = candidate.split("\n")[0];
    }
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

function getProductIdFromMatches(productIdMatches, matchIndex = undefined) {
  if (productIdMatches === null) {
    return null;
  }
  if (matchIndex !== undefined) {
    return productIdMatches[matchIndex];
  }
  for (var i = productIdMatches.length - 1; i >= 0; i--) {
    if (productIdMatches[i] !== undefined) {
      return productIdMatches[i];
    }
  }
  return null;
}

function getProductIdWithPattern(sourceMap, patternMap) {
  const hostname = window.location.hostname;
  for (const sourceName of Object.keys(sourceMap)) {
    if (patternMap[sourceName] === undefined ||
      !(hostname in patternMap[sourceName])) {
      continue;
    }
    const source = sourceMap[sourceName];
    const heuristic = patternMap[sourceName][hostname];
    if (Array.isArray(heuristic)) {
      return getProductIdFromMatches(source.match(
        new RegExp(heuristic[0], 'i')), heuristic[1]);
    } else {
      return getProductIdFromMatches(source.match(
        new RegExp(heuristic, 'i')));
    }
  }
  return null;
}

function extractProductId(url, imageUrl, item) {
  const idExtractionMapNotExist =
    typeof idExtractionMap === 'undefined' ||
    idExtractionMap === undefined;
  const couponIdExtractionMapNotExist =
    typeof couponIdExtractionMap === 'undefined' ||
    couponIdExtractionMap === undefined;
  if (idExtractionMapNotExist && couponIdExtractionMapNotExist) {
    return null;
  }
  let productId = null;
  const sourceMap = {"product_url": url,
    "product_image_url": imageUrl,
    "product_element": item.outerHTML};
  if (!idExtractionMapNotExist) {
    productId = getProductIdWithPattern(sourceMap, idExtractionMap);
    if (productId !== null) return productId;
  }
  if (!couponIdExtractionMapNotExist) {
    productId = getProductIdWithPattern(sourceMap, couponIdExtractionMap);
    if (productId !== null) return productId;
  }
  return null;
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
  let price = extractPrice(item);
  // eBay "You may also like" and "Guides" are not product items.
  // Not having price is one hint.
  // FIXME: "Also viewed" items in Gap doesn't have prices.
  if (price == null) {
    if (verbose > 0)
      console.warn('no price found', item);
    return null;
  }
  let extractionResult =
      {'url': url, 'imageUrl': imageUrl, 'title': title, 'price': price};
  // productId is an optional field for extraction.
  const productId = extractProductId(url, imageUrl, item);
  if (productId !== null) {
    extractionResult['productId'] = productId;
  }
  return extractionResult;
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

function matchPattern(item, pattern, matchText) {
  if (item === null) return false;
  const textToMatch = matchText ? item.textContent : item.outerHTML;
  return textToMatch.toLowerCase().match(pattern);
}

function isCartItem(item) {
  // TODO: Improve the heuristic here to accommodate more formats of cart item.
  if (matchPattern(item, moveToCartTextRegex, true)) return false;
  // Walmart has 'move to cart' outside of the div.cart-item.
  if (matchPattern(item.parentElement, moveToCartTextRegex, true)) return false;
  if (matchPattern(item, cartPriceTextRegex, true)) return false;
  // Item element in bestbuy.com contains "add to cart" for things
  // like protection plans.
  if (!document.URL.includes("bestbuy.com")
      && !document.URL.includes("orientaltrading.com")
      && matchPattern(item, addToCartTextRegex, true)) return false;
  if ((document.URL.includes("ashleyfurniture.com")
      || document.URL.includes("gnc.com")
      || document.URL.includes("bathandbodyworks.com"))
      && matchPattern(item, minicartHTMLRegex, false)) return false;
  if (document.URL.includes("ashleyfurniture.com")
      && matchPattern(item, cartItemQtyRegex, true) === null)
    return false;
  return matchPattern(item, cartItemTextRegex, true) ||
    matchPattern(item, cartItemQtyRegex, true) ||
    matchPattern(item, cartItemHTMLRegex, false);
}

function extractOneItem(item, extracted_items, processed, output,
  savedForLaterSection, skipFiltering) {
  if (skipFiltering) {
    const extraction = extractItem(item);
    if (extraction != null) {
      output.set(item, extraction);
      extracted_items.push(item);
    }
    return;
  }
  if (verbose > 1) {
    console.log('trying', item);
  }
  if (item.childElementCount == 0 && item.parentElement.tagName != 'BODY') {
    // Amazone store page uses overlay <a>.
    item = item.parentElement;
    if (item == null)
      return;
  }
  if (processed.has(item)) {
    if (verbose > 0)
      console.log('processed', item);
    return;
  }
  processed.add(item);
  if (item.scrollHeight > 1000) {
    if (verbose > 0)
      console.log('too tall', item);
    return;
  }
  if (hasOverlap(item, extracted_items)) {
    if (verbose > 0)
      console.log('overlap', item);
    return;
  }
  // scrollHeight could be 0 while getBoundingClientRect().height > 0.
  const bounding_rect = item.getBoundingClientRect();
  if (bounding_rect.height < 50) {
    if (verbose > 0)
      console.log('too short', item);
    return;
  }
  if (bounding_rect.height * bounding_rect.width > 800 * window.innerWidth) {
    if (verbose > 0)
      console.log('too tall', item);
    return;
  }
  if (item.querySelectorAll('img, amp-img, .bg-img').length == 0) {
    if (verbose > 0)
      console.log('no image', item);
    return;
  }
  if (!item.textContent.match(priceRegex)) {
    if (verbose > 0)
      console.log('no price', item);
    return;
  }
  if (bounding_rect.top <= 10 &&
      (document.URL.includes('partycity.com') ||
       document.URL.includes('chewy.com'))) {
    if (verbose > 0)
      console.log('likely cart page header', item);
    return;
  }
  if (isInSavedForLater(item, savedForLaterSection)) {
    if (verbose > 0)
      console.log('in save for later', item);
    return;
  }
  if (!isCartItem(item)) {
    if (verbose > 0)
      console.log('not cart item', item);
    return;
  }
  if (verbose > 0)
    console.log('try extracting', item);
  const extraction = extractItem(item);
  if (extraction != null) {
    output.set(item, extraction);
    extracted_items.push(item);
  }
}

function isInSavedForLater(item, savedForLaterSection) {
  return savedForLaterSection !== null
    && savedForLaterSection.getBoundingClientRect().top
    < item.getBoundingClientRect().top
    && !item.textContent.toLowerCase().match(saveForLaterRegex);
}

function getSavedForLaterSection() {
  // This regex should match the XPath pattern below.
  const shortCutRegex = new RegExp(
      '(your saved items)|(saved for later)|(my saved items)|(wishlist items)',
      'i');
  if (!document.body.innerText.match(shortCutRegex))
    return null;

  const nodes = document.evaluate(
    "//*[contains(translate(" +
    "text(), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz'), " +
    "'your saved items')" +
    "or contains(translate(" +
    "text(), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz'), " +
    "'saved for later')" +
    "or contains(translate(" +
    "text(), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz'), " +
    "'my saved items')" +
    "or contains(translate(" +
    "text(), 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz'), " +
    "'wishlist items')]", document,
  null, XPathResult.ORDERED_NODE_ITERATOR_TYPE, null);
  let node = nodes.iterateNext();
  let section = null;
  while (node) {
    if (node!= null && node.offsetHeight >= 1 && node.offsetWidth >= 1) {
      section = node;
    }
    node = nodes.iterateNext();
  }
  return section
}

function isHeuristicsImprovementEnabled() {
  if (typeof isImprovementEnabled === 'undefined'
    || typeof isImprovementEnabled !== 'boolean') {
    return false;
  }
  return isImprovementEnabled;
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

// Remove duplicate products with identical product URLs.
function deduplicateResults(output) {
  if (!document.URL.includes("sourcebmx.com")) return output;
  const productUrls = new Set();
  let filteredOutput = [];
  for (let i = 0; i < output.length; i++) {
    const productUrl = output[i]["url"];
    if (!productUrls.has(productUrl)) {
      filteredOutput.push(output[i]);
      productUrls.add(productUrl)
    }
  }
  return filteredOutput;
}

if (typeof Sleeper === 'undefined') {
  var Sleeper = class {
    constructor() {
      // 99.9th percentile of the individual task execution times should be
      // < 50ms.
      // The task time is defined as exclusive CPU usage, from last time
      // sleeping is done to the beginning of the next sleep.
      let min_task_time = 10;
      if (typeof kSleeperMinTaskTimeMs !== 'undefined') {
        min_task_time = kSleeperMinTaskTimeMs;
      }
      this.min_task_time = min_task_time;

      // Avoid monopolizing JavaScript main thread execution time.
      let duty_cycle = 0.05;
      if (typeof kSleeperDutyCycle !== 'undefined') {
        duty_cycle = kSleeperDutyCycle;
      }
      this.duty_cycle = Math.max(0.01, Math.min(duty_cycle, 1));

      this.last_sleep = performance.now();
      this.start = performance.now();
      this.longest_task = 0;
      this.total_tasks_time = 0;
    }

    async maybeSleep() {
      const elapsed = performance.now() - this.last_sleep;
      if (elapsed <= this.min_task_time)
        return;
      this.longest_task = Math.max(this.longest_task, elapsed);
      this.total_tasks_time += elapsed;
      if (verbose > 1) {
        console.log('longest task', this.longest_task);
      }

      // Calculate the delay aiming for the target duty cycle.
      // duty_cycle = (working time) / (working time + sleeping time)
      //            = elapsed / (elapsed + delay)
      const delay = elapsed * (1 - this.duty_cycle) / this.duty_cycle;
      await new Promise(r => setTimeout(r, delay));
      this.last_sleep = performance.now();
    }

    get longestTask() {
      const elapsed = performance.now() - this.last_sleep;
      return Math.max(this.longest_task, elapsed);
    }

    get totalTasksTime() {
      const elapsed = performance.now() - this.last_sleep;
      return this.total_tasks_time + elapsed;
    }

    get elapsed() {
      return performance.now() - this.start;
    }
  }
}

async function extractAllItems(root) {
  let timeout = 250;
  if (typeof kTimeoutMs !== 'undefined') {
    timeout = kTimeoutMs;
  }

  let items = [];
  const sleeper = new Sleeper();
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
  let skipFiltering = true;
  if (document.URL.includes("kiehls.com")
    || document.URL.includes("laroche-posay.us")) {
    items = root.querySelectorAll(".c-product-table__row");
  } else if (document.URL.includes("americastire.com")
    || document.URL.includes("discounttire.com")) {
    items = root.querySelectorAll("[role=\"listitem\"]");
  } else if (document.URL.includes("discounttiredirect.com")) {
    items = root.querySelectorAll(".cart-item");
  } else if (document.URL.includes("shutterfly.com")){
    items = root.querySelectorAll(".cartitem");
  } else {
    skipFiltering = false;
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
  }
  await sleeper.maybeSleep();

  if (verbose > 0)
    console.log(items);
  const outputMap = new Map();
  const processed = new Set();
  const extracted_items = [];
  let savedForLaterSection = null;
  if (isHeuristicsImprovementEnabled()) {
    savedForLaterSection = getSavedForLaterSection();
    if (verbose > 0)
      console.log(savedForLaterSection);
    await sleeper.maybeSleep();
  }

  let i = 0;
  let early_abort = false;
  for (const item of items) {
    extractOneItem(item, extracted_items, processed, outputMap,
      savedForLaterSection, skipFiltering);
    // Checking for every item is too slow.
    if (i++ % 10 == 0) {
      await sleeper.maybeSleep();
      if (sleeper.totalTasksTime > timeout) {
        if (verbose > 0) {
          console.log('aborted due to timeout');
        }
        early_abort = true;
        break;
      }
    }
  }

  const keysInDocOrder =
      Array.from(outputMap.keys()).sort(documentPositionComparator);
  const output = [];
  for (const key of keysInDocOrder) {
    output.push(outputMap.get(key));
  }
  await sleeper.maybeSleep();
  return {
    'products': deduplicateResults(output),
    'longest_task_ms': sleeper.longestTask,
    'total_tasks_ms': sleeper.totalTasksTime,
    'elapsed_ms': sleeper.elapsed,
    'timedout': early_abort,
  };
}

extracted_results_promise = extractAllItems(document);
