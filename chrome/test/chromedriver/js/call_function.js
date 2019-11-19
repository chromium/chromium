// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for WebDriver status codes.
 * @enum {number}
 */
var StatusCode = {
  STALE_ELEMENT_REFERENCE: 10,
  JAVA_SCRIPT_ERROR: 17,
};

/**
 * Enum for node types.
 * @enum {number}
 */
var NodeType = {
  ELEMENT: 1,
  DOCUMENT: 9,
};

/**
 * Dictionary key to use for holding an element ID.
 * @const
 * @type {string}
 */
var ELEMENT_KEY = 'ELEMENT';

/**
 * True if using W3C Element references.
 * @const
 * @type {boolean}
 */
var w3cEnabled = false;

/**
 * True if shadow dom is enabled.
 * @const
 * @type {boolean}
 */
var SHADOW_DOM_ENABLED = typeof ShadowRoot === 'function';

/**
 * Generates a unique ID to identify an element.
 * @void
 * @return {string} Randomly generated ID.
 */
function generateUUID() {
  var array = new Uint8Array(16);
  window.crypto.getRandomValues(array);
  array[6] = 0x40 | (array[6] & 0x0f);
  array[8] = 0x80 | (array[8] & 0x3f);

  var UUID = "";
  for (var i = 0; i < 16; i++) {
    var temp = array[i].toString(16);
    if (temp.length < 2)
      temp = "0" + temp;
    UUID += temp;
    if (i == 3 || i == 5 || i == 7 || i == 9)
      UUID += "-";
  }
  return UUID;
};

/**
 * Constructs new error to be thrown with given code and message.
 * @param {string} message Message reported to user.
 * @param {StatusCode} code StatusCode for error.
 * @return {!Error} Error object that can be thrown.
 */
function newError(message, code) {
  const error = new Error(message);
  error.code = code;
  return error;
}

/**
 * A cache which maps IDs <-> cached objects for the purpose of identifying
 * a script object remotely. Uses UUIDs for identification.
 * @constructor
 */
function CacheWithUUID() {
  this.cache_ = Object.create(null);
}

CacheWithUUID.prototype = {
  /**
   * Stores a given item in the cache and returns a unique UUID.
   *
   * @param {!Object} item The item to store in the cache.
   * @return {number} The UUID for the cached item.
   */
  storeItem: function(item) {
    for (var i in this.cache_) {
      if (item == this.cache_[i])
        return i;
    }
    var id = generateUUID();
    this.cache_[id] = item;
    return id;
  },

  /**
   * Retrieves the cached object for the given ID.
   *
   * @param {number} id The ID for the cached item to retrieve.
   * @return {!Object} The retrieved item.
   */
  retrieveItem: function(id) {
    var item = this.cache_[id];
    if (item)
      return item;
    throw newError('element is not attached to the page document',
                   StatusCode.STALE_ELEMENT_REFERENCE);
  },

  /**
   * Clears stale items from the cache.
   */
  clearStale: function() {
    for (var id in this.cache_) {
      var node = this.cache_[id];
      if (!this.isNodeReachable_(node))
        delete this.cache_[id];
    }
  },

  /**
    * @private
    * @param {!Node} node The node to check.
    * @return {boolean} If the nodes is reachable.
    */
  isNodeReachable_: function(node) {
    var nodeRoot = getNodeRootThroughAnyShadows(node);
    return (nodeRoot == document);
  }


};

/**
 * A cache which maps IDs <-> cached objects for the purpose of identifying
 * a script object remotely.
 * @constructor
 */
function Cache() {
  this.cache_ = Object.create(null);
  this.nextId_ = 1;
  this.idPrefix_ = Math.random().toString();
}

Cache.prototype = {

  /**
   * Stores a given item in the cache and returns a unique ID.
   *
   * @param {!Object} item The item to store in the cache.
   * @return {number} The ID for the cached item.
   */
  storeItem: function(item) {
    for (var i in this.cache_) {
      if (item == this.cache_[i])
        return i;
    }
    var id = this.idPrefix_  + '-' + this.nextId_;
    this.cache_[id] = item;
    this.nextId_++;
    return id;
  },

  /**
   * Retrieves the cached object for the given ID.
   *
   * @param {number} id The ID for the cached item to retrieve.
   * @return {!Object} The retrieved item.
   */
  retrieveItem: function(id) {
    var item = this.cache_[id];
    if (item)
      return item;
    throw newError('element is not attached to the page document',
                   StatusCode.STALE_ELEMENT_REFERENCE);
  },

  /**
   * Clears stale items from the cache.
   */
  clearStale: function() {
    for (var id in this.cache_) {
      var node = this.cache_[id];
      if (!this.isNodeReachable_(node))
        delete this.cache_[id];
    }
  },

  /**
    * @private
    * @param {!Node} node The node to check.
    * @return {boolean} If the nodes is reachable.
    */
  isNodeReachable_: function(node) {
    var nodeRoot = getNodeRootThroughAnyShadows(node);
    return (nodeRoot == document);
  }
};

/**
 * Returns the root element of the node.  Found by traversing parentNodes until
 * a node with no parent is found.  This node is considered the root.
 * @param {?Node} node The node to find the root element for.
 * @return {?Node} The root node.
 */
function getNodeRoot(node) {
  while (node && node.parentNode) {
    node = node.parentNode;
  }
  return node;
}

/**
 * Returns the root element of the node, jumping up through shadow roots if
 * any are found.
 */
function getNodeRootThroughAnyShadows(node) {
  var root = getNodeRoot(node);
  while (SHADOW_DOM_ENABLED && root instanceof ShadowRoot) {
    root = getNodeRoot(root.host);
  }
  return root;
}

/**
 * Returns the global object cache for the page.
 * @param {Document=} opt_doc The document whose cache to retrieve. Defaults to
 *     the current document.
 * @return {!Cache} The page's object cache.
 */
function getPageCache(opt_doc, opt_w3c) {
  var doc = opt_doc || document;
  var w3c = opt_w3c || false;
  // |key| is a long random string, unlikely to conflict with anything else.
  var key = '$cdc_asdjflasutopfhvcZLmcfl_';
  if (w3c) {
    if (!(key in doc))
      doc[key] = new CacheWithUUID();
    return doc[key];
  } else {
    if (!(key in doc))
      doc[key] = new Cache();
    return doc[key];
  }
}

/**
 * Returns whether given value is an element.
 * @param {*} value The value to identify as object.
 * @return {boolean} True if value is a cacheable element.
 */
function isElement(value) {
  // As of crrev.com/1316933002, typeof() for some elements will return
  // 'function', not 'object'. So we need to check for both non-null objects, as
  // well Elements that also happen to be callable functions (e.g. <embed> and
  // <object> elements). Note that we can not use |value instanceof Object| here
  // since this does not work with frames/iframes, for example
  // frames[0].document.body instanceof Object == false even though
  // typeof(frames[0].document.body) == 'object'.
  return ((typeof(value) == 'object' && value != null) ||
            (typeof(value) == 'function' && value.nodeName &&
            value.nodeType == NodeType.ELEMENT)) &&
          (value.nodeType == NodeType.ELEMENT   ||
           value.nodeType == NodeType.DOCUMENT  ||
           (SHADOW_DOM_ENABLED && value instanceof ShadowRoot));
}

/**
 * Returns whether given value is a collection (iterable with
 * 'length' property).
 * @param {*} value The value to identify as a collection.
 * @return {boolean} True if value is an iterable collection.
 */
function isCollection(value) {
  return (typeof value[Symbol.iterator] === 'function');
}

/**
 * Deep-clones item, given object references in seen, using cloning algorithm
 * algo. Implements "clone an object" from W3C-spec (#dfn-clone-an-object).
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} seen Object references that have already been seen.
 * @param {function(*, Array<*>, ?Cache) : *} algo Cloning algorithm to use to
 *     deep clone properties of item.
 * @param {?Cache} opt_cache Optional cache to use for cloning.
 * @return {*} Clone of item with status of cloning.
 */
function cloneWithAlgorithm(item, seen, algo, opt_cache) {
  let tmp = null;
  function maybeCopyProperty(prop) {
    let sourceValue = null;
    try {
      sourceValue = item[prop];
    } catch(e) {
      throw newError('error reading property', StatusCode.JAVA_SCRIPT_ERROR);
    }
    return algo(sourceValue, seen, opt_cache);
  }

  if (isCollection(item)) {
    tmp = new Array(item.length);
    for (let i = 0; i < item.length; ++i)
      tmp[i] = maybeCopyProperty(i);
  } else {
    tmp = {};
    for (let prop in item)
      tmp[prop] = maybeCopyProperty(prop);
  }
  return tmp;
}

/**
 * Wrapper to cloneWithAlgorithm, with circular reference detection logic.
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} seen Object references that have already been seen.
 * @param {function(*, Array<*>, ?Cache) : *} algo Cloning algorithm to use to
 *     deep clone properties of item.
 * @return {*} Clone of item with status of cloning.
 */
function cloneWithCircularCheck(item, seen, algo) {
  if (seen.includes(item))
    throw newError('circular reference', StatusCode.JAVA_SCRIPT_ERROR);
  seen.push(item);
  const result = cloneWithAlgorithm(item, seen, algo);
  seen.pop();
  return result;
}

/**
 * Returns deep clone of given value, replacing element references with a
 * serialized string representing that element.
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} seen Object references that have already been seen.
 * @return {*} Clone of item with status of cloning.
 */
function jsonSerialize(item, seen) {
  if (item === undefined || item === null)
    return null;
  if (typeof item === 'boolean' ||
      typeof item === 'number' ||
      typeof item === 'string')
    return item;
  if (isElement(item)) {
    const root = getNodeRootThroughAnyShadows(item);
    const cache = getPageCache(root, w3cEnabled);
    if (!cache.isNodeReachable_(item))
      throw newError('stale element not found',
                     StatusCode.STALE_ELEMENT_REFERENCE);
    const ret = {};
    ret[ELEMENT_KEY] = cache.storeItem(item);
    return ret;
  }
  if (isCollection(item))
    return cloneWithCircularCheck(item, seen, jsonSerialize);
  // http://crbug.com/chromedriver/2995: Placed here because some element
  // (above) are type 'function', so this check must be performed after.
  if (typeof item === 'function')
    return item;
  // TODO(rohpavone): Implement WindowProxy serialization.
  if (typeof item.toJSON === 'function' &&
      (item.hasOwnProperty('toJSON') ||
       Object.getPrototypeOf(item).hasOwnProperty('toJSON')))
    return item.toJSON();

  // Deep clone Objects.
  return cloneWithCircularCheck(item, seen, jsonSerialize);
}

/**
 * Returns deserialized deep clone of given value, replacing serialized string
 * references to elements with a element reference, if found.
 * @param {*} item Object or collection to deep clone.
 * @param {?Array<*>} opt_seen Object references that have already been seen.
 * @param {?Cache} opt_cache Document cache containing serialized elements.
 * @return {*} Clone of item with status of cloning.
 */
function jsonDeserialize(item, opt_seen, opt_cache) {
  if (opt_seen === undefined || opt_seen === null)
    opt_seen = []
  if (item === undefined ||
      item === null ||
      typeof item === 'boolean' ||
      typeof item === 'number' ||
      typeof item === 'string' ||
      typeof item === 'function')
    return item;
  if (item.hasOwnProperty(ELEMENT_KEY)) {
    if (opt_cache === undefined || opt_cache === null) {
      const root = getNodeRootThroughAnyShadows(item);
      opt_cache = getPageCache(root, w3cEnabled);
    }
    return  opt_cache.retrieveItem(item[ELEMENT_KEY]);
  }
  if (isCollection(item) || typeof item === 'object')
    return cloneWithAlgorithm(item, opt_seen, jsonDeserialize, opt_cache);
  throw newError('unhandled object', StatusCode.JAVA_SCRIPT_ERROR);
}

/**
 * Calls a given function and returns its value.
 *
 * The inputs to and outputs of the function will be unwrapped and wrapped
 * respectively, unless otherwise specified. This wrapping involves converting
 * between cached object reference IDs and actual JS objects. The cache will
 * automatically be pruned each call to remove stale references.
 *
 * @param {function(...[*]) : *} func The function to invoke.
 * @param {!Array<*>} args The array of arguments to supply to the function,
 *     which will be unwrapped before invoking the function.
 * @param {boolean} w3c Whether to return a W3C compliant element reference.
 * @param {boolean=} opt_unwrappedReturn Whether the function's return value
 *     should be left unwrapped.
 * @return {*} An object containing a status and value property, where status
 *     is a WebDriver status code and value is the wrapped value. If an
 *     unwrapped return was specified, this will be the function's pure return
 *     value.
 */
function callFunction(func, args, w3c, opt_unwrappedReturn) {
  if (w3c) {
    w3cEnabled = true;
    ELEMENT_KEY = 'element-6066-11e4-a52e-4f735466cecf';

  }
  const cache = getPageCache(null, w3cEnabled);
  cache.clearStale();

  function buildError(error) {
    return {
      status: error.code || StatusCode.JAVA_SCRIPT_ERROR,
      value: error.message || error
    };
  }

  let status = 0;
  let returnValue;
  try {
    const unwrappedArgs = jsonDeserialize(args, [], cache);
    const tmp = func.apply(null, unwrappedArgs);
    return Promise.resolve(tmp).then((result) => {
      if (opt_unwrappedReturn)
        return result;
      const clone = jsonSerialize(result, []);
      return {
        status: 0,
        value: clone
      };
    }).catch(buildError);
  } catch (error) {
    return Promise.resolve(buildError(error));
  }
}
