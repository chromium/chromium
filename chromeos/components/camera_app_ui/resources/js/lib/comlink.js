/**
 * Copyright 2019 Google Inc. All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* eslint-disable */
const proxyMarker = Symbol('Comlink.proxy');
const createEndpoint = Symbol('Comlink.endpoint');
const releaseProxy = Symbol('Comlink.releaseProxy');
const throwSet = new WeakSet();
const transferHandlers = new Map([
  [
    'proxy', {
      canHandle: obj => obj && obj[proxyMarker],
      serialize(obj) {
        const {port1, port2} = new MessageChannel();
        expose(obj, port1);
        return [port2, [port2]];
      },
      deserialize: (port) => {
        port.start();
        return wrap(port);
      }
    }
  ],
  [
    'throw', {
      canHandle: obj => throwSet.has(obj),
      serialize(obj) {
        const isError = obj instanceof Error;
        let serialized = obj;
        if (isError) {
          serialized = {isError, message: obj.message, stack: obj.stack};
        }
        return [serialized, []];
      },
      deserialize(obj) {
        if (obj.isError) {
          throw Object.assign(new Error(), obj);
        }
        throw obj;
      }
    }
  ]
]);
function expose(obj, ep = self) {
  ep.addEventListener('message', function callback(ev) {
    if (!ev || !ev.data) {
      return;
    }
    const {id, type, path} = Object.assign({path: []}, ev.data);
    const argumentList = (ev.data.argumentList || []).map(fromWireValue);
    let returnValue;
    try {
      const parent = path.slice(0, -1).reduce((obj, prop) => obj[prop], obj);
      const rawValue = path.reduce((obj, prop) => obj[prop], obj);
      switch (type) {
        case 0 /* GET */: {
          returnValue = rawValue;
        } break;
        case 1 /* SET */: {
          parent[path.slice(-1)[0]] = fromWireValue(ev.data.value);
          returnValue = true;
        } break;
        case 2 /* APPLY */: {
          returnValue = rawValue.apply(parent, argumentList);
        } break;
        case 3 /* CONSTRUCT */: {
          const value = new rawValue(...argumentList);
          returnValue = proxy(value);
        } break;
        case 4 /* ENDPOINT */: {
          const {port1, port2} = new MessageChannel();
          expose(obj, port2);
          returnValue = transfer(port1, [port1]);
        } break;
        case 5 /* RELEASE */: {
          returnValue = undefined;
        } break;
      }
    } catch (e) {
      returnValue = e;
      throwSet.add(e);
    }
    Promise.resolve(returnValue)
        .catch(e => {
          throwSet.add(e);
          return e;
        })
        .then(returnValue => {
          const [wireValue, transferables] = toWireValue(returnValue);
          ep.postMessage(
              Object.assign(Object.assign({}, wireValue), {id}), transferables);
          if (type === 5 /* RELEASE */) {
            // detach and deactive after sending release response above.
            ep.removeEventListener('message', callback);
            closeEndPoint(ep);
          }
        });
  });
  if (ep.start) {
    ep.start();
  }
}
function isMessagePort(endpoint) {
  return endpoint.constructor.name === 'MessagePort';
}
function closeEndPoint(endpoint) {
  if (isMessagePort(endpoint))
    endpoint.close();
}
function wrap(ep, target = undefined) {
  return createProxy(ep, [], target);
}
function throwIfProxyReleased(isReleased) {
  if (isReleased) {
    throw new Error('Proxy has been released and is not useable');
  }
}
function createProxy(ep, path = [], target = function() {}) {
  let isProxyReleased = false;
  const proxy = new Proxy(target, {
    get(_target, prop) {
      throwIfProxyReleased(isProxyReleased);
      if (prop === releaseProxy) {
        return () => {
          return requestResponseMessage(
                     ep,
                     {type: 5 /* RELEASE */, path: path.map(p => p.toString())})
              .then(() => {
                closeEndPoint(ep);
                isProxyReleased = true;
              });
        };
      }
      if (prop === 'then') {
        if (path.length === 0) {
          return {then: () => proxy};
        }
        const r = requestResponseMessage(ep, {
                    type: 0 /* GET */,
                    path: path.map(p => p.toString())
                  }).then(fromWireValue);
        return r.then.bind(r);
      }
      return createProxy(ep, [...path, prop]);
    },
    set(_target, prop, rawValue) {
      throwIfProxyReleased(isProxyReleased);
      // FIXME: ES6 Proxy Handler `set` methods are supposed to return a
      // boolean. To show good will, we return true asynchronously ¯\_(ツ)_/¯
      const [value, transferables] = toWireValue(rawValue);
      return requestResponseMessage(
                 ep, {
                   type: 1 /* SET */,
                   path: [...path, prop].map(p => p.toString()),
                   value
                 },
                 transferables)
          .then(fromWireValue);
    },
    apply(_target, _thisArg, rawArgumentList) {
      throwIfProxyReleased(isProxyReleased);
      const last = path[path.length - 1];
      if (last === createEndpoint) {
        return requestResponseMessage(ep, {
                 type: 4 /* ENDPOINT */
               })
            .then(fromWireValue);
      }
      // We just pretend that `bind()` didn’t happen.
      if (last === 'bind') {
        return createProxy(ep, path.slice(0, -1));
      }
      const [argumentList, transferables] = processArguments(rawArgumentList);
      return requestResponseMessage(
                 ep, {
                   type: 2 /* APPLY */,
                   path: path.map(p => p.toString()),
                   argumentList
                 },
                 transferables)
          .then(fromWireValue);
    },
    construct(_target, rawArgumentList) {
      throwIfProxyReleased(isProxyReleased);
      const [argumentList, transferables] = processArguments(rawArgumentList);
      return requestResponseMessage(
                 ep, {
                   type: 3 /* CONSTRUCT */,
                   path: path.map(p => p.toString()),
                   argumentList
                 },
                 transferables)
          .then(fromWireValue);
    }
  });
  return proxy;
}
function myFlat(arr) {
  return Array.prototype.concat.apply([], arr);
}
function processArguments(argumentList) {
  const processed = argumentList.map(toWireValue);
  return [processed.map(v => v[0]), myFlat(processed.map(v => v[1]))];
}
const transferCache = new WeakMap();
function transfer(obj, transfers) {
  transferCache.set(obj, transfers);
  return obj;
}

/**
 * @template T
 * @param {T} obj
 * @return {T}
 */
function proxy(obj) {
  return Object.assign(obj, {[proxyMarker]: true});
}
function windowEndpoint(w, context = self, targetOrigin = '*') {
  return {
    postMessage: (msg, transferables) =>
        w.postMessage(msg, targetOrigin, transferables),
    addEventListener: context.addEventListener.bind(context),
    removeEventListener: context.removeEventListener.bind(context)
  };
}
function toWireValue(value) {
  for (const [name, handler] of transferHandlers) {
    if (handler.canHandle(value)) {
      const [serializedValue, transferables] = handler.serialize(value);
      return [
        {type: 3 /* HANDLER */, name, value: serializedValue}, transferables
      ];
    }
  }
  return [{type: 0 /* RAW */, value}, transferCache.get(value) || []];
}
function fromWireValue(value) {
  switch (value.type) {
    case 3 /* HANDLER */:
      return transferHandlers.get(value.name).deserialize(value.value);
    case 0 /* RAW */:
      return value.value;
  }
}
function requestResponseMessage(ep, msg, transfers = undefined) {
  return new Promise(resolve => {
    const id = generateUUID();
    ep.addEventListener('message', function l(ev) {
      if (!ev.data || !ev.data.id || ev.data.id !== id) {
        return;
      }
      ep.removeEventListener('message', l);
      resolve(ev.data);
    });
    if (ep.start) {
      ep.start();
    }
    ep.postMessage(Object.assign({id}, msg), transfers);
  });
}
function generateUUID() {
  return new Array(4)
      .fill(0)
      .map(
          () =>
              Math.floor(Math.random() * Number.MAX_SAFE_INTEGER).toString(16))
      .join('-');
}

export {
  createEndpoint,
  expose,
  proxy,
  proxyMarker,
  releaseProxy,
  transfer,
  transferHandlers,
  windowEndpoint,
  wrap
};
