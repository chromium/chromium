// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxy} from './browser_proxy/browser_proxy.js';
import {assertInstanceof} from './chrome_util.js';
import {ErrorLevel, ErrorType, reportError} from './error.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {Facing} from './type.js';

/**
 * Creates a canvas element for 2D drawing.
 * @param {{width: number, height: number}} params Width/Height of the canvas.
 * @return {{canvas: !HTMLCanvasElement, ctx: !CanvasRenderingContext2D}}
 *     Returns canvas element and the context for 2D drawing.
 */
export function newDrawingCanvas({width, height}) {
  const canvas =
      assertInstanceof(document.createElement('canvas'), HTMLCanvasElement);
  canvas.width = width;
  canvas.height = height;
  const ctx =
      assertInstanceof(canvas.getContext('2d'), CanvasRenderingContext2D);
  return {canvas, ctx};
}

/**
 * Gets the clockwise rotation and flip that can orient a photo to its upright
 * position of the photo and drop its orientation information.
 * @param {!Blob} blob JPEG blob that might contain EXIF orientation field.
 * @return {!Promise<{rotation: number, flip: boolean, blob: !Blob}>} The
 * rotation, flip information of photo and the photo blob after drop those
 * information.
 */
function dropPhotoOrientation(blob) {
  let /** !Blob */ blobWithoutOrientation = blob;
  const getOrientation = (async () => {
    const buffer = await blob.arrayBuffer();
    const view = new DataView(buffer);
    if (view.getUint16(0, false) !== 0xFFD8) {
      return 1;
    }
    const length = view.byteLength;
    let offset = 2;
    while (offset < length) {
      if (view.getUint16(offset + 2, false) <= 8) {
        break;
      }
      const marker = view.getUint16(offset, false);
      offset += 2;
      if (marker === 0xFFE1) {
        if (view.getUint32(offset += 2, false) !== 0x45786966) {
          break;
        }

        const little = view.getUint16(offset += 6, false) === 0x4949;
        offset += view.getUint32(offset + 4, little);
        const tags = view.getUint16(offset, little);
        offset += 2;
        for (let i = 0; i < tags; i++) {
          if (view.getUint16(offset + (i * 12), little) === 0x0112) {
            offset += (i * 12) + 8;
            const orientation = view.getUint16(offset, little);
            view.setUint16(offset, 1, little);
            blobWithoutOrientation = new Blob([buffer]);
            return orientation;
          }
        }
      } else if ((marker & 0xFF00) !== 0xFF00) {
        break;
      } else {
        offset += view.getUint16(offset, false);
      }
    }
    return 1;
  })();

  return getOrientation
      .then((orientation) => {
        switch (orientation) {
          case 1:
            return {rotation: 0, flip: false};
          case 2:
            return {rotation: 0, flip: true};
          case 3:
            return {rotation: 180, flip: false};
          case 4:
            return {rotation: 180, flip: true};
          case 5:
            return {rotation: 90, flip: true};
          case 6:
            return {rotation: 90, flip: false};
          case 7:
            return {rotation: 270, flip: true};
          case 8:
            return {rotation: 270, flip: false};
          default:
            return {rotation: 0, flip: false};
        }
      })
      .then((orientInfo) => {
        return Object.assign(orientInfo, {blob: blobWithoutOrientation});
      });
}

/**
 * Orients a photo to the upright orientation.
 * @param {!Blob} blob Photo as a blob.
 * @param {function(!Blob)} onSuccess Success callback with the result photo as
 *     a blob.
 * @param {function()} onFailure Failure callback.
 */
export function orientPhoto(blob, onSuccess, onFailure) {
  // TODO(shenghao): Revise or remove this function if it's no longer
  // applicable.
  const drawPhoto = function(original, orientation, onSuccess, onFailure) {
    const canvasSquareLength = Math.max(original.width, original.height);
    const {canvas, ctx} = newDrawingCanvas(
        {width: canvasSquareLength, height: canvasSquareLength});

    const [centerX, centerY] = [canvas.width / 2, canvas.height / 2];
    ctx.translate(centerX, centerY);
    ctx.rotate(orientation.rotation * Math.PI / 180);
    if (orientation.flip) {
      ctx.scale(-1, 1);
    }
    ctx.drawImage(
        original, -original.width / 2, -original.height / 2, original.width,
        original.height);
    if (orientation.flip) {
      ctx.scale(-1, 1);
    }
    ctx.rotate(-orientation.rotation * Math.PI / 180);
    ctx.translate(-centerX, -centerY);

    const outputSize = (() => {
      if (orientation.rotation === 90 || orientation.rotation === 270) {
        return {width: original.height, height: original.width};
      } else {
        return {width: original.width, height: original.height};
      }
    })();
    const {canvas: outputCanvas, ctx: outputCtx} = newDrawingCanvas(outputSize);
    const imageData = ctx.getImageData(
        (canvasSquareLength - outputCanvas.width) / 2,
        (canvasSquareLength - outputCanvas.height) / 2, outputCanvas.width,
        outputCanvas.height);
    outputCtx.putImageData(imageData, 0, 0);

    outputCanvas.toBlob(function(blob) {
      if (blob) {
        onSuccess(blob);
      } else {
        onFailure();
      }
    }, 'image/jpeg');
  };

  dropPhotoOrientation(blob).then((orientation) => {
    if (orientation.rotation === 0 && !orientation.flip) {
      onSuccess(blob);
    } else {
      const original =
          assertInstanceof(document.createElement('img'), HTMLImageElement);
      original.onload = function() {
        drawPhoto(original, orientation, onSuccess, onFailure);
      };
      original.onerror = onFailure;
      original.src = URL.createObjectURL(orientation.blob);
    }
  });
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
  return new Promise((resolve, reject) => {
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
 * @param {!Node} rootElement Root of DOM subtree to be set up
 *     with.
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
 * @param {string} url Picture as an URL.
 * @param {boolean} isVideo Picture is a video.
 * @param {number} width Target width to be scaled to.
 * @param {number=} height Target height to be scaled to. In default, set to
 *     corresponding rounded height with respect to target width and aspect
 *     ratio of input picture.
 * @return {!Promise<!Blob>} Promise for the result.
 */
export async function scalePicture(url, isVideo, width, height = undefined) {
  const element =
      /** @type {(!HTMLImageElement|!HTMLVideoElement)} */ (
          document.createElement(isVideo ? 'video' : 'img'));
  if (isVideo) {
    element.preload = 'auto';
  }
  await new Promise((resolve, reject) => {
    element.addEventListener(isVideo ? 'canplay' : 'load', resolve);
    element.addEventListener('error', reject);
    element.src = url;
  });
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
    reportError(
        ErrorType.BROKEN_THUMBNAIL, ErrorLevel.ERROR,
        new Error('The thumbnail content is broken.'));
    // Do not throw an error here. A black thumbnail is still better than no
    // thumbnail to let user open the corresponding picutre in gallery.
  }

  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) {
        resolve(blob);
      } else {
        reject(new Error('Failed to create thumbnail.'));
      }
    }, 'image/jpeg');
  });
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
  state.addObserver(s, (value) => {
    const label = value ? onLabel : offLabel;
    element.setAttribute('i18n-label', label);
    element.setAttribute('aria-label', browserProxy.getI18nMessage(label));
  });
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
