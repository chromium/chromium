// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enum for WebDriver status codes.
 * @enum {number}
 */
const StatusCode = {
  STALE_ELEMENT_REFERENCE: 10,
  JAVA_SCRIPT_ERROR: 17,
  NO_SUCH_SHADOW_ROOT: 65,
  DETACHED_SHADOW_ROOT: 66
};

/**
 * Enum for node types.
 * @enum {number}
 */
const NodeType = {
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
 * Dictionary key to use for holding a shadow element ID.
 * @const
 * @type {string}
 */
const SHADOW_ROOT_KEY = 'shadow-6066-11e4-a52e-4f735466cecf';
const W3C_ELEMENT_KEY = 'element-6066-11e4-a52e-4f735466cecf';

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
const SHADOW_DOM_ENABLED = typeof ShadowRoot === 'function';

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

function isNodeReachable(node) {
  const nodeRoot = getNodeRootThroughAnyShadows(node);
  return (nodeRoot == document.documentElement.parentNode);
}

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
  let root = getNodeRoot(node);
  while (SHADOW_DOM_ENABLED && root instanceof ShadowRoot) {
    root = getNodeRoot(root.host);
  }
  return root;
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
  const Symbol = window.cdc_adoQpoasnfa76pfcZLmcfl_Symbol || window.Symbol;
  return (typeof value[Symbol.iterator] === 'function') &&
      ('length' in value) &&
      (typeof value.length === 'number');
}

/**
 * Deep-clones item, given object references in seen, using cloning algorithm
 * algo. Implements "clone an object" from W3C-spec (#dfn-clone-an-object).
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} seen Object references that have already been seen.
 * @param {function(*, Array<*>) : *} algo Cloning algorithm to use to
 *     deep clone properties of item.
 * @param {!Array<*>} nodes List of serialized nodes
 * @return {*} Clone of item with status of cloning.
 */
function cloneWithAlgorithm(item, seen, algo, nodes) {
  let tmp = null;
  function maybeCopyProperty(prop) {
    let sourceValue = null;
    try {
      sourceValue = item[prop];
    } catch(e) {
      throw newError('error reading property', StatusCode.JAVA_SCRIPT_ERROR);
    }
    return algo(sourceValue, seen, nodes);
  }

  if (isCollection(item)) {
    const Array = window.cdc_adoQpoasnfa76pfcZLmcfl_Array || window.Array;
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
 * @param {function(*, Array<*>) : *} algo Cloning algorithm to use to
 *     deep clone properties of item.
 * @param {!Array<*>} nodes List of serialized nodes
 * @return {*} Clone of item with status of cloning.
 */
function cloneWithCircularCheck(item, seen, algo, nodes) {
  if (seen.includes(item))
    throw newError('circular reference', StatusCode.JAVA_SCRIPT_ERROR);
  seen.push(item);
  const result = cloneWithAlgorithm(item, seen, algo, nodes);
  seen.pop();
  return result;
}

/*
 * Prohibits call of object.prototype.toJSoN()
 */
function serializationGuard(object) {
  const handler = {
    get(target, name) {
      const value = target[name]
      if (typeof value !== 'function')
        return value;
      // Objects that have own toJSON are never guarded with a proxy.
      // All other functions are replaced with {} in preprocessResult.
      // The only remaining case when a client tries to access a method is a
      // call to non-own toJSON by JSON.stringify.
      // In this case this method needs to be concealed.
      return undefined;
    }
  }
  const Proxy = window.cdc_adoQpoasnfa76pfcZLmcfl_Proxy || window.Proxy;
  return new Proxy(object, handler);
}



/**
 * Returns deep clone of given value, replacing element references with a
 * serialized string representing that element.
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} seen Object references that have already been seen.
 * @param {!Array<*>} nodes List of serialized nodes
 * @return {*} Clone of item with status of cloning.
 */
function preprocessResult(item, seen, nodes) {
  if (item === undefined || item === null)
    return null;
  if (typeof item === 'boolean' ||
      typeof item === 'number' ||
      typeof item === 'string')
    return item;
  // We never descend to own property toJSON.
  // Any other function must be serialized as an object.
  if (typeof item === 'function')
    return {};
  if (isElement(item)) {
    if (!isNodeReachable(item)) {
      if (item instanceof ShadowRoot)
        throw newError('shadow root is detached from the current frame',
            StatusCode.DETACHED_SHADOW_ROOT);
      throw newError('stale element not found in the current frame',
                     StatusCode.STALE_ELEMENT_REFERENCE);
    }
    const ret = {};
    let key = ELEMENT_KEY;
    if (item instanceof ShadowRoot) {
      if (!item.nodeType ||
          item.nodeType !== item.DOCUMENT_FRAGMENT_NODE ||
          !item.host) {
        throw newError('no such shadow root', StatusCode.NO_SUCH_SHADOW_ROOT);
      }
      key = SHADOW_ROOT_KEY;
    }
    ret[key] = nodes.length;
    nodes.push(item);
    return serializationGuard(ret);
  }

  // TODO(crbug.com/40229283): Implement WindowProxy serialization.

  if (Object.hasOwn(item, 'toJSON') && typeof item.toJSON === 'function') {
      // Not guarded because we want item.toJSON to be invoked by
      // JSON.stringify.
      return item;
  }

  // Deep cloning of Array and Objects.
  return serializationGuard(
      cloneWithCircularCheck(item, seen, preprocessResult, nodes));
}

/**
 * Returns deserialized deep clone of given value, replacing serialized string
 * references to elements with a element reference, if found.
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} seen Object references that have already been seen.
 * @param {!Array<*>} nodes List of referred nodes
 * @return {*} Clone of item with status of cloning.
 */
function resolveReferencesRecursive(item, seen, nodes) {
  if (item === undefined ||
      item === null ||
      typeof item === 'boolean' ||
      typeof item === 'number' ||
      typeof item === 'string' ||
      typeof item === 'function')
    return item;
  if (item.hasOwnProperty(ELEMENT_KEY) ||
      item.hasOwnProperty(SHADOW_ROOT_KEY)) {
    let idx = item[ELEMENT_KEY];
    if (item.hasOwnProperty(SHADOW_ROOT_KEY))
      idx = item[SHADOW_ROOT_KEY];
    if (idx < 0 || idx >= nodes.length) {
      throw newError('unable to resove node reference. '
          + 'Node index is out of range.', StatusCode.JAVA_SCRIPT_ERROR);
    }
    return nodes[idx];
  }
  if (isCollection(item) || typeof item === 'object')
    return cloneWithAlgorithm(item, seen, resolveReferencesRecursive, nodes);
  throw newError('unhandled object', StatusCode.JAVA_SCRIPT_ERROR);
}

/**
 * Returns deserialized deep clone of given value, replacing serialized string
 * references to elements with a element reference, if found.
 * @param {*} item Object or collection to deep clone.
 * @param {!Array<*>} nodes List of referred nodes
 * @return {*} Clone of item with status of cloning.
 */
function resolveReferences(args, nodes) {
  for (let idx = 0; idx < nodes.length; ++idx) {
    if (!isNodeReachable(nodes[idx])) {
      if (nodes[idx] instanceof ShadowRoot)
        throw newError('shadow root is detached from the current frame',
            StatusCode.DETACHED_SHADOW_ROOT);
      throw newError('stale element not found in the current frame',
                     StatusCode.STALE_ELEMENT_REFERENCE);
    }
  }
  return resolveReferencesRecursive(args, [], nodes);
}

/**
 * Calls a given function and returns its value.
 *
 * The inputs to and outputs of the function will be unwrapped and wrapped
 * respectively, unless otherwise specified. This wrapping involves converting
 * between cached object reference IDs and actual JS objects.
 *
 * @param {function(...[*]) : *} func The function to invoke.
 * @param {!Array<*>} args The array of arguments to supply to the function,
 *     which will be unwrapped before invoking the function.
 * @param {boolean} w3c Whether to return a W3C compliant element reference.
 * @param {!Array<*>} Nodes referred in the arguments.
 * @return {*} An object containing a status and value property, where status
 *     is a WebDriver status code and value is the wrapped value. If an
 *     unwrapped return was specified, this will be the function's pure return
 *     value.
 */
function callFunction(func, args, w3c, nodes) {
  if (w3c) {
    w3cEnabled = true;
    ELEMENT_KEY = W3C_ELEMENT_KEY;

  }

  function buildError(error) {
    const errorResponse = serializationGuard({
      status: error.code || StatusCode.JAVA_SCRIPT_ERROR,
      value: error.message || error
    });
    const JSON = window.cdc_adoQpoasnfa76pfcZLmcfl_JSON || window.JSON;
    return [JSON.stringify(errorResponse)];
  }

  function wrapErrorAsJavascriptError(error) {
    originalResponse = buildError(error);
    originalStatus = error.code || StatusCode.JAVA_SCRIPT_ERROR;
    if (originalStatus === StatusCode.JAVA_SCRIPT_ERROR) {
      return originalResponse;
    }
    return buildError({
      code: StatusCode.JAVA_SCRIPT_ERROR,
      message: originalResponse[0]});
  }

  const Promise = window.cdc_adoQpoasnfa76pfcZLmcfl_Promise || window.Promise;
  let unwrappedArgs = null;
  try {
    unwrappedArgs = resolveReferences(args, nodes);
  } catch (error) {
    return Promise.resolve(buildError(error));
  }

  try {
    const tmp = func.apply(null, unwrappedArgs);
    return Promise.resolve(tmp).then((result) => {
      ret_nodes = [];
      const response = {
        status: 0,
        value: preprocessResult(result, [], ret_nodes)
      };
      const JSON = window.cdc_adoQpoasnfa76pfcZLmcfl_JSON || window.JSON;
      return [JSON.stringify(response), ...ret_nodes];
    }).catch(wrapErrorAsJavascriptError);
  } catch (error) {
    return Promise.resolve(wrapErrorAsJavascriptError(error));
  }
}
