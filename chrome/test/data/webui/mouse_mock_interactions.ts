// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface Point {
  x: number;
  y: number;
}

/**
 * Returns the (x,y) coordinates representing the middle of a node.
 */
export function middleOfNode(node: HTMLElement): Point {
  const rect = node.getBoundingClientRect();
  return {y: rect.top + (rect.height / 2), x: rect.left + (rect.width / 2)};
}

/**
 * Returns the (x,y) coordinates representing the top left corner of a node.
 */
export function topLeftOfNode(node: HTMLElement): Point {
  const rect = node.getBoundingClientRect();
  return {y: rect.top, x: rect.left};
}

/**
 * Fires a mouse event on a specific node, at a given set of coordinates.
 * This event bubbles and is cancellable.
 *
 * @param type The type of mouse event (such as 'mouseup' or 'mousedown').
 * @param xy The (x,y) coordinates the mouse event should be fired from.
 */
function dispatchMouseEvent(type: string, xy: Point, node: HTMLElement) {
  node.dispatchEvent(new MouseEvent(type, {
    bubbles: true,
    cancelable: true,
    composed: true,

    clientX: xy.x,
    clientY: xy.y,
    buttons: 1,
  }));
}

/**
 * Simulates a mouse move action by firing a `move` mouse event on a
 * specific node, between a set of coordinates.
 *
 * @param from The (x,y) coordinates the dragging should start from.
 * @param to The (x,y) coordinates the dragging should end at.
 * @param steps The numbers of steps in the move motion.
 */
export function move(
    node: HTMLElement, from: Point, to: Point, steps: number = 5) {
  const dx = Math.round((from.x - to.x) / steps);
  const dy = Math.round((from.y - to.y) / steps);
  const xy = {x: from.x, y: from.y};
  for (let i = steps; i > 0; i--) {
    dispatchMouseEvent('mousemove', xy, node);
    xy.x += dx;
    xy.y += dy;
  }
  dispatchMouseEvent('mousemove', to, node);
}

/**
 * Simulates a mouse dragging action originating in the middle of a specific
 * node.
 *
 * @param dx The horizontal displacement.
 * @param dy The vertical displacement
 * @param steps The numbers of steps in the dragging motion.
 */
export function track(
    target: HTMLElement, dx: number = 0, dy: number = 0, steps = 5) {
  down(target);
  const start = middleOfNode(target);
  const end = {x: start.x + dx, y: start.y + dy};
  move(target, start, end, steps);
  up(target, end);
}

/**
 * Fires a `mousedown` MouseEvent on a specific node, at a given set of
 * coordinates. This event bubbles and is cancellable. If the (x,y) coordinates
 * are not specified, the middle of the node will be used instead.
 */
export function down(node: HTMLElement, xy?: Point) {
  dispatchMouseEvent('mousedown', xy || middleOfNode(node), node);
}

/**
 * Fires a `mouseup` MouseEvent on a specific node, at a given set of
 * coordinates. This event bubbles and is cancellable. If the (x,y) coordinates
 * are not specified, the middle of the node will be used instead.
 */
export function up(node: HTMLElement, xy?: Point) {
  dispatchMouseEvent('mouseup', xy || middleOfNode(node), node);
}
