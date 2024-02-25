// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Make all transitions and animations take 0ms. NOTE: this will completely
 * disable webkitTransitionEnd events. If your code relies on them firing, it
 * will break. animationend events should still work.
 */
export function disableAnimationsAndTransitions(): void {
  const all = document.body.querySelectorAll<HTMLElement>('*');
  const ZERO_MS_IMPORTANT = '0ms !important';
  for (let i = 0; i < all.length; ++i) {
    const style = all[i]!.style;
    style.animationDelay = ZERO_MS_IMPORTANT;
    style.animationDuration = ZERO_MS_IMPORTANT;
    style.transitionDelay = ZERO_MS_IMPORTANT;
    style.transitionDuration = ZERO_MS_IMPORTANT;
  }

  const realElementAnimate = Element.prototype.animate;
  Element.prototype.animate = function(
      keyframes: Keyframe[]|PropertyIndexedKeyframes|null,
      options?: number|KeyframeAnimationOptions) {
    if (typeof options === 'object') {
      options.duration = 0;
    } else {
      options = 0;
    }
    return realElementAnimate.call(this, keyframes, options);
  };
}
