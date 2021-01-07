// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assertInstanceof} from './chrome_util.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import * as Comlink from './lib/comlink.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  UntrustedOrigin,  // eslint-disable-line no-unused-vars
} from './type.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Creates a canvas element for 2D drawing.
 * @param {{width: number, height: number}} params Width/Height of the canvas.
 * @return {{canvas: !HTMLCanvasElement, ctx: !CanvasRenderingContext2D}}
 *     Returns canvas element and the context for 2D drawing.
 */
export function newDrawingCanvas({width, height}) {
  const canvas = dom.create('canvas', HTMLCanvasElement);
  canvas.width = width;
  canvas.height = height;
  const ctx =
      assertInstanceof(canvas.getContext('2d'), CanvasRenderingContext2D);
  return {canvas, ctx};
}

/**
 * Cancels animating the element by removing 'animate' class.
 * @param {!HTMLElement} element Element for canceling animation.
 * @return {!Promise} Promise resolved when ongoing animation is canceled and
 *     next animation can be safely applied.
 */
export function animateCancel(element) {
  element.classList.remove('animate');
  element.classList.add('cancel-animate');
  /** @suppress {suspiciousCode} */
  element.offsetWidth;  // Force calculation to re-apply animation.
  element.classList.remove('cancel-animate');
  // Assumes transitioncancel, transitionend, animationend events from previous
  // animation are all cleared after requestAnimationFrame().
  return new Promise((r) => requestAnimationFrame(r));
}

/**
 * Waits for animation completed.
 * @param {!HTMLElement} element Element to be animated.
 * @return {!Promise} Promise is resolved when animation is completed or
 *     cancelled.
 */
function waitAnimationCompleted(element) {
  return new Promise((resolve) => {
    let animationCount = 0;
    const onStart = (event) =>
        void (event.target === element && animationCount++);
    const onFinished = (event, callback) => {
      if (event.target !== element || --animationCount !== 0) {
        return;
      }
      events.forEach(([e, fn]) => element.removeEventListener(e, fn));
      callback();
    };
    const events = [
      ['transitionrun', onStart], ['animationstart', onStart],
      ['transitionend', (event) => onFinished(event, resolve)],
      ['animationend', (event) => onFinished(event, resolve)],
      ['transitioncancel', (event) => onFinished(event, resolve)],
      // animationcancel is not implemented on chrome.
    ];
    events.forEach(([e, fn]) => element.addEventListener(e, fn));
  });
}

/**
 * Animates the element once by applying 'animate' class.
 * @param {!HTMLElement} element Element to be animated.
 * @param {function()=} callback Callback called on completion.
 */
export function animateOnce(element, callback) {
  animateCancel(element).then(() => {
    element.classList.add('animate');
    waitAnimationCompleted(element).finally(() => {
      element.classList.remove('animate');
      if (callback) {
        callback();
      }
    });
  });
}

/**
 * Returns a shortcut string, such as Ctrl-Alt-A.
 * @param {!KeyboardEvent} event Keyboard event.
 * @return {string} Shortcut identifier.
 */
export function getShortcutIdentifier(event) {
  let identifier = (event.ctrlKey ? 'Ctrl-' : '') +
      (event.altKey ? 'Alt-' : '') + (event.shiftKey ? 'Shift-' : '') +
      (event.metaKey ? 'Meta-' : '');
  if (event.key) {
    switch (event.key) {
      case 'ArrowLeft':
        identifier += 'Left';
        break;
      case 'ArrowRight':
        identifier += 'Right';
        break;
      case 'ArrowDown':
        identifier += 'Down';
        break;
      case 'ArrowUp':
        identifier += 'Up';
        break;
      case 'a':
      case 'p':
      case 's':
      case 'v':
      case 'r':
        identifier += event.key.toUpperCase();
        break;
      default:
        identifier += event.key;
    }
  }
  return identifier;
}

/**
 * Makes the element unfocusable by mouse.
 * @param {!HTMLElement} element Element to be unfocusable.
 */
export function makeUnfocusableByMouse(element) {
  element.addEventListener('mousedown', (event) => event.preventDefault());
}

/**
 * Opens help.
 */
export function openHelp() {
  window.open(
      'https://support.google.com/chromebook/?p=camera_usage_on_chromebook');
}

/**
 * Sets up i18n messages on DOM subtree by i18n attributes.
 * @param {!Element|!DocumentFragment} rootElement Root of DOM subtree to be set
 *     up with.
 */
export function setupI18nElements(rootElement) {
  const getElements = (attr) => rootElement.querySelectorAll('[' + attr + ']');
  const getMessage = (element, attr) =>
      browserProxy.getI18nMessage(element.getAttribute(attr));
  const setAriaLabel = (element, attr) =>
      element.setAttribute('aria-label', getMessage(element, attr));

  getElements('i18n-content')
      .forEach(
          (element) => element.textContent =
              getMessage(element, 'i18n-content'));
  getElements('i18n-tooltip-true')
      .forEach(
          (element) => element.setAttribute(
              'tooltip-true', getMessage(element, 'i18n-tooltip-true')));
  getElements('i18n-tooltip-false')
      .forEach(
          (element) => element.setAttribute(
              'tooltip-false', getMessage(element, 'i18n-tooltip-false')));
  getElements('i18n-aria')
      .forEach((element) => setAriaLabel(element, 'i18n-aria'));
  tooltip.setup(getElements('i18n-label'))
      .forEach((element) => setAriaLabel(element, 'i18n-label'));
}

/**
 * Reads blob into Image.
 * @param {!Blob} blob
 * @return {!Promise<!HTMLImageElement>}
 * @throws {!Error}
 */
export function blobToImage(blob) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('Failed to load unprocessed image'));
    img.src = URL.createObjectURL(blob);
  });
}

/**
 * Gets default facing according to device mode.
 * @return {!Facing}
 */
export function getDefaultFacing() {
  return state.get(state.State.TABLET) ? Facing.ENVIRONMENT : Facing.USER;
}

/**
 * Scales the input picture to target width and height with respect to original
 * aspect ratio.
 * @param {!Blob} blob Blob of photo or video to be scaled.
 * @param {boolean} isVideo Picture is a video.
 * @param {number} width Target width to be scaled to.
 * @param {number=} height Target height to be scaled to. In default, set to
 *     corresponding rounded height with respect to target width and aspect
 *     ratio of input picture.
 * @return {!Promise<!Blob>} Promise for the result.
 */
export async function scalePicture(blob, isVideo, width, height = undefined) {
  const element = isVideo ? dom.create('video', HTMLVideoElement) :
                            dom.create('img', HTMLImageElement);
  try {
    let requestFrameTimeout = false;
    if (isVideo) {
      await new Promise((resolve, reject) => {
        element.addEventListener('error', () => {
          let msg = 'Failed to load video';
          /**
           * https://developer.mozilla.org/en-US/docs/Web/API/HTMLMediaElement/error
           * @type {?MediaError}
           */
          const err = element.error;
          if (err !== null) {
            msg += `: ${err.message}`;
          }
          reject(new Error(msg));
        });
        /**
         * For resolving https://goo.gl/LdLk22 asynchronous play-pause problem.
         * @type {!Promise}
         */
        let playing = Promise.resolve();
        Promise
            .race([
              new Promise(
                  (resolve) =>
                      element.requestVideoFrameCallback(() => resolve(false))),
              // The |requestVideoFrameCallback| may not be triggerred when
              // playing malformatted video. Set 300ms timeout here to prevent
              // UI be blocked forever.
              new Promise((resolve) => setTimeout(() => resolve(true), 300)),
            ])
            .then((isTimeout) => {
              requestFrameTimeout = isTimeout;
              return playing;
            })
            .then(() => {
              element.pause();
              resolve();
            });
        element.preload = 'auto';
        element.src = URL.createObjectURL(blob);
        playing = assertInstanceof(element.play(), Promise);
      });
    } else {
      await new Promise((resolve, reject) => {
        element.addEventListener(
            'error', () => reject(new Error('Failed to load image')));
        element.addEventListener('load', resolve);
        element.src = URL.createObjectURL(blob);
      });
    }
    if (height === undefined) {
      const ratio = isVideo ? element.videoHeight / element.videoWidth :
                              element.height / element.width;
      height = Math.round(width * ratio);
    }
    const {canvas, ctx} = newDrawingCanvas({width, height});
    ctx.drawImage(element, 0, 0, width, height);

    /**
     * @type {!Uint8ClampedArray} A one-dimensional pixels array in RGBA order.
     */
    const data = ctx.getImageData(0, 0, width, height).data;
    if (data.every((byte) => byte === 0)) {
      let msg =
          `The ${isVideo ? 'video' : 'photo'} thumbnail content is broken.`;
      if (requestFrameTimeout) {
        msg += ' ; while requestVideoFrameCallback is timeout.';
      }
      reportError(
          ErrorType.BROKEN_THUMBNAIL,
          ErrorLevel.ERROR,
          new Error(msg),
      );
      // Do not throw an error here. A black thumbnail is still better than no
      // thumbnail to let user open the corresponding picutre in gallery.
    }

    return new Promise((resolve) => {
      // TODO(b/174190121): Patch important exif entries from input blob to
      // result blob.
      canvas.toBlob(resolve, 'image/jpeg');
    });
  } finally {
    URL.revokeObjectURL(element.src);
  }
}

/**
 * Toggle checked value of element.
 * @param {!HTMLInputElement} element
 * @param {boolean} checked
 */
export function toggleChecked(element, checked) {
  element.checked = checked;
  element.dispatchEvent(new Event('change'));
}

/**
 * Binds on/off of specified state with different aria label on an element.
 * @param {{element: !Element, state: !state.State, onLabel: string,
 *     offLabel: string}} params
 */
export function bindElementAriaLabelWithState(
    {element, state: s, onLabel, offLabel}) {
  const update = (value) => {
    const label = value ? onLabel : offLabel;
    element.setAttribute('i18n-label', label);
    element.setAttribute('aria-label', browserProxy.getI18nMessage(label));
  };
  update(state.get(s));
  state.addObserver(s, update);
}

/**
 * Sets inkdrop effect on button or label in setting menu.
 * @param {!HTMLElement} el
 */
export function setInkdropEffect(el) {
  el.addEventListener('click', (e) => {
    const tRect =
        assertInstanceof(e.target, HTMLElement).getBoundingClientRect();
    const elRect = el.getBoundingClientRect();
    const dropX = tRect.left + e.offsetX - elRect.left;
    const dropY = tRect.top + e.offsetY - elRect.top;
    const maxDx = Math.max(Math.abs(dropX), Math.abs(elRect.width - dropX));
    const maxDy = Math.max(Math.abs(dropY), Math.abs(elRect.height - dropY));
    const radius = Math.hypot(maxDx, maxDy);
    el.style.setProperty('--drop-x', `${dropX}px`);
    el.style.setProperty('--drop-y', `${dropY}px`);
    el.style.setProperty('--drop-radius', `${radius}px`);
    animateOnce(el);
  });
}

/**
 * Instantiates template with the target selector.
 * @param {string} selector
 * @return {!DocumentFragment}
 */
export function instantiateTemplate(selector) {
  const tpl = dom.get(selector, HTMLTemplateElement);
  const doc = assertInstanceof(
      document.importNode(tpl.content, true), DocumentFragment);
  setupI18nElements(doc);
  return doc;
}

/**
 * Creates JS module by given |scriptUrl| under untrusted context with given
 * origin and returns its proxy.
 * @param {string} scriptUrl The URL of the script to load.
 * @param {!UntrustedOrigin} origin The origin of the untrusted context.
 * @return {!Promise<!Object>}
 */
export async function createUntrustedJSModule(scriptUrl, origin) {
  const untrustedPageReady = new WaitableEvent();
  const iFrame = dom.create('iframe', HTMLIFrameElement);
  iFrame.addEventListener('load', () => untrustedPageReady.signal());
  iFrame.setAttribute('src', `${origin}/views/untrusted_script_loader.html`);
  iFrame.hidden = true;
  document.body.appendChild(iFrame);
  await untrustedPageReady.wait();

  const untrustedRemote =
      await Comlink.wrap(Comlink.windowEndpoint(iFrame.contentWindow, self));
  await untrustedRemote.loadScript(scriptUrl);
  return untrustedRemote;
}
