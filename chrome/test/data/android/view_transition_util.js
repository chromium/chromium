// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let transition = null;

addEventListener('pagereveal', e => {
  if (e.viewTransition) {
    _transitionDidStart(e.viewTransition);
  }
});

// Allow tests to control when the transition starts. Tests call
// startTransitionAnimation() to complete the author DOM callback and allow the
// view-transition to start the animation.
let startTransitionAnimation = null;
let startPromise =
    new Promise(resolve => {startTransitionAnimation = resolve;});

// Allow tests to wait until the snapshot has been taken and is ready to start
// transitioning. Tests use readyToStartPromise to wait until the the author
// updateDOM callback has been invoked.
let readyToStartResolve = null;
let readyToStartPromise =
    new Promise(resolve => {readyToStartResolve = resolve;});

// Individual test files set this to perform the DOM update to the new
// transition state.
let updateDOM = null;

// Sets the animation time to just before the end (not the end itself) to
// prevent finishing the animation. Since the animations have steps timing
// function the state is equivalent to the end state itself.
function animateToEndState() {
  if (transition == null)
    throw new Error('Transition was already finished or never started.');

  for (const anim of document.getAnimations())
    anim.currentTime = anim.effect.getTiming().duration - 1;
}

// Finishes animations, and thus the view transition.
function finishAnimations() {
  if (transition == null)
    throw new Error('Transition was already finished or never started.');

  for (const anim of document.getAnimations())
    anim.finish();
}

// Creates a view transition. The transition will call the test's defined
// updateDOM function to mutate the DOM into the new state and resolve the
// readyToStartPromise when the DOM has been updated. The animation won't start
// until the test calls startTransitionAnimation().
function createTransition() {
  if (transition != null)
    throw new Error('In-progress transition already exists.');
  if (updateDOM == null)
    throw new Error('Test must set an updateDOM function');

  let t = document.startViewTransition(() => {
    updateDOM();

    readyToStartResolve();

    return startPromise;
  });

  _transitionDidStart(t);
}

// Lets the harness know about an active transition. This will cause its
// animations to be initially paused.
function _transitionDidStart(t) {
  transition = t;

  // Initially pause the animation at the old state so the test can take a
  // screenshot. Tests can then use moveAnimationToNewState() to play the
  // animation forward to the new state.
  transition.ready.then(() => {
    for (const anim of document.getAnimations()) {
      anim.pause();
    }
  });

  transition.finished.then( () => {
    transition = null;
  });
}
