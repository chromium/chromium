// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from './animation.js';
import {assertInstanceof} from './chrome_util.js';
import * as dom from './dom.js';
import * as Comlink from './lib/comlink.js';
import * as loadTimeData from './models/load_time_data.js';
import * as state from './state.js';
import * as tooltip from './tooltip.js';
import {
  Facing,
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
  const getElements = (attr) =>
      dom.getAllFrom(rootElement, '[' + attr + ']', HTMLElement);
  const getMessage = (element, attr) =>
      loadTimeData.getI18nMessage(element.getAttribute(attr));
  const setAriaLabel = (element, attr) =>
      element.setAttribute('aria-label', getMessage(element, attr));

  getElements('i18n-text')
      .forEach(
          (element) => element.textContent = getMessage(element, 'i18n-text'));
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
    element.setAttribute('aria-label', loadTimeData.getI18nMessage(label));
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
    animate.play(el);
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
 * @return {!Promise<!Object>}
 */
export async function createUntrustedJSModule(scriptUrl) {
  const untrustedPageReady = new WaitableEvent();
  const iFrame = dom.create('iframe', HTMLIFrameElement);
  iFrame.addEventListener('load', () => untrustedPageReady.signal());
  iFrame.setAttribute(
      'src',
      'chrome-untrusted://camera-app/views/untrusted_script_loader.html');
  iFrame.hidden = true;
  document.body.appendChild(iFrame);
  await untrustedPageReady.wait();

  const untrustedRemote =
      await Comlink.wrap(Comlink.windowEndpoint(iFrame.contentWindow, self));
  await untrustedRemote.loadScript(scriptUrl);
  return untrustedRemote;
}

/**
 * Sleeps for a specified time.
 * @param {number} ms Milliseconds to sleep.
 * @return {!Promise}
 */
export function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
