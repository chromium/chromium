// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for behavior specific to Controller.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ControllerTest {
    // Convenience method to create a scope that mutates a list of strings on state transitions.
    // When entering the state, it will append "enter ${id} ${data}" to the result list, where
    // `data` is the String that is associated with the state activation. When exiting the state,
    // it will append "exit ${id}" to the result list. This provides a readable way to track and
    // verify the behavior of observers in response to the Observables they are linked to.
    public static <T> Observer<T> report(List<String> result, String id) {
        // Did you know that lambdas are awesome.
        return (T data) -> {
            result.add("enter " + id + ": " + data);
            return () -> result.add("exit " + id);
        };
    }

    @Test
    public void testNoStateTransitionAfterRegisteringWithInactiveController() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        recorder.verify().end();
    }

    @Test
    public void testStateIsopenedWhenControllerIsSet() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        // Activate the state by setting the controller.
        controller.set("cool");
        recorder.verify().opened("cool").end();
    }

    @Test
    public void testBasicStateFromController() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set("fun");
        // Deactivate the state by resetting the controller.
        controller.reset();
        recorder.verify().opened("fun").closed("fun").end();
    }

    @Test
    public void testSetStateTwicePerformsImplicitReset() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        // Activate the state for the first time.
        controller.set("first");
        // Activate the state for the second time.
        controller.set("second");
        // If set() is called without a reset() in-between, the tracking state exits, then re-enters
        // with the new data. So we expect to find an "exit" call between the two enter calls.
        recorder.verify().opened("first").closed("first").opened("second").end();
    }

    @Test
    public void testResetWhileStateIsNotopenedIsNoOp() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.reset();
        recorder.verify().end();
    }

    @Test
    public void testMultipleStatesObservingSingleController() {
        // Construct two states that subscribe the same Controller. Verify both observers' events
        // are triggered.
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder1 = ReactiveRecorder.record(controller);
        ReactiveRecorder recorder2 = ReactiveRecorder.record(controller);
        // Activate the controller, which should propagate a state transition to both states.
        // Both states should be updated, so we should get two enter events.
        controller.set("neat");
        controller.reset();
        recorder1.verify().opened("neat").closed("neat").end();
        recorder2.verify().opened("neat").closed("neat").end();
    }

    @Test
    public void testNewStateIsActivatedImmediatelyIfObservingAlreadyActiveObservable() {
        Controller<String> controller = new Controller<>();
        controller.set("surprise");
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        recorder.verify().opened("surprise").end();
    }

    @Test
    public void testNewStateIsNotActivatedIfObservingObservableThatHasBeenDeactivated() {
        Controller<String> controller = new Controller<>();
        controller.set("surprise");
        controller.reset();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        recorder.verify().end();
    }

    @Test
    public void testResetWhileAlreadyDeactivatedIsANoOp() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set("radical");
        controller.reset();
        // Resetting again after already resetting should not notify the observer.
        controller.reset();
        recorder.verify().opened("radical").closed("radical").end();
    }

    @Test
    public void testClosedSubscriptionDoesNotGetNotifiedOfFutureActivations() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        a.set("during temp");
        a.reset();
        recorder.unsubscribe();
        a.set("after temp");
        recorder.verify().opened("during temp").closed("during temp").end();
    }

    @Test
    public void testClosedSubscriptionIsImplicitlyDeactivated() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        a.set("implicitly reset this");
        recorder.unsubscribe();
        recorder.verify().opened("implicitly reset this").closed("implicitly reset this").end();
    }

    @Test
    public void testCloseSubscriptionAfterDeactivatingSourceStateDoesNotCallExitHAndlerAgain() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        a.set("and a one");
        a.reset();
        recorder.unsubscribe();
        recorder.verify().opened("and a one").closed("and a one").end();
    }

    @Test
    public void testSetControllerWithNullImplicitlyResets() {
        Controller<String> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        a.set("not null");
        a.set(null);
        recorder.verify().opened("not null").closed("not null").end();
    }

    @Test
    public void testResetControllerInActivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.subscribe((String s) -> {
            result.add("enter " + s);
            a.reset();
            result.add("after reset");
            return () -> {
                result.add("exit");
            };
        });
        a.set("immediately retracted");
        assertThat(result, contains("enter immediately retracted", "after reset", "exit"));
    }

    @Test
    public void testSetControllerInActivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.subscribe(report(result, "weirdness"));
        a.subscribe((String s) -> {
            // If the activation handler always calls set() on the source controller, you will have
            // an infinite loop, which is not cool. However, if the activation handler only
            // conditionally calls set() on its source controller, then the case where set() is not
            // called will break the loop. It is the responsibility of the programmer to solve the
            // halting problem for activation handlers.
            if (s.equals("first")) {
                a.set("second");
            }
            return () -> {
                result.add("haha");
            };
        });
        a.set("first");
        assertThat(result,
                contains("enter weirdness: first", "haha", "exit weirdness",
                        "enter weirdness: second"));
    }

    @Test
    public void testResetControllerInDeactivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.subscribe(report(result, "bizzareness"));
        a.subscribe((String s) -> () -> a.reset());
        a.set("yo");
        a.reset();
        // The reset() called by the deactivation handler should be a no-op.
        assertThat(result, contains("enter bizzareness: yo", "exit bizzareness"));
    }

    @Test
    public void testSetControllerInDeactivationHandler() {
        Controller<String> a = new Controller<>();
        List<String> result = new ArrayList<>();
        a.subscribe(report(result, "astoundingness"));
        a.subscribe((String s) -> () -> a.set("never mind"));
        a.set("retract this");
        a.reset();
        // The set() called by the deactivation handler should immediately set the controller back.
        assertThat(result,
                contains("enter astoundingness: retract this", "exit astoundingness",
                        "enter astoundingness: never mind"));
    }

    @Test
    public void testSetWithDuplicateValueIsNoOp() {
        Controller<String> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set("stop copying me");
        controller.set("stop copying me");
        recorder.verify().opened("stop copying me").end();
    }

    @Test
    public void testSetUnitControllerInActivatedStateIsNoOp() {
        Controller<Unit> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller);
        controller.set(Unit.unit());
        recorder.verify().opened(Unit.unit()).end();
        controller.set(Unit.unit());
        recorder.verify().end();
    }
}
