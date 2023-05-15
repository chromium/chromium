// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.emptyIterable;
import static org.hamcrest.Matchers.not;

import org.junit.Assert;

import java.util.ArrayList;
import java.util.List;

/**
 * Records events emitted by Observables, and provides a fluent interface to perform assertions on
 * the received events. Use this in unit tests to get descriptive output for assertion failures.
 */
public class ReactiveRecorder {
    private final List<Event> mRecord;
    private final Scope mSubscription;

    public static ReactiveRecorder record(Observable<?> observable) {
        return new ReactiveRecorder(observable);
    }

    private ReactiveRecorder(Observable<?> observable) {
        mRecord = new ArrayList<>();
        mSubscription = observable.subscribe((Object value) -> {
            mRecord.add(openEvent(value));
            return () -> mRecord.add(closeEvent(value));
        });
    }

    public void unsubscribe() {
        mSubscription.close();
    }

    public ReactiveRecorder reset() {
        mRecord.clear();
        return this;
    }

    public Validator verify() {
        return new Validator();
    }

    /**
     * The fluent interface used to perform assertions. Each opened() or closed() call pops the
     * least-recently-added event from the record, and verifies that it meets the description
     * provided by the arguments given to opened() or closed(). Use end() to assert that no more
     * events were received.
     */
    public class Validator {
        private Validator() {}

        public Validator opened(Object value) {
            Event event = pop();
            event.checkType("open");
            event.checkValue(value);
            return this;
        }

        public Validator closed(Object value) {
            Event event = pop();
            event.checkType("close");
            event.checkValue(value);
            return this;
        }

        public void end() {
            assertThat(mRecord, emptyIterable());
        }
    }

    private Event pop() {
        assertThat(mRecord, not(emptyIterable()));
        return mRecord.remove(0);
    }

    private Event openEvent(Object value) {
        Event result = new Event();
        result.type = "open";
        result.value = value;
        return result;
    }

    private Event closeEvent(Object value) {
        Event result = new Event();
        result.type = "close";
        result.value = value;
        return result;
    }

    private static class Event {
        public String type;
        public Object value;

        private Event() {}

        public void checkType(String type) {
            Assert.assertEquals("Event " + this + " is not an " + type + " event", type, this.type);
        }

        public void checkValue(Object value) {
            Assert.assertEquals(
                    "Event " + this + " has wrong value, expected " + value, value, this.value);
        }

        @Override
        public String toString() {
            return "(" + type + ": " + value + ")";
        }
    }
}
