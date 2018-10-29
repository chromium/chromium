// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.task.test.ShadowAsyncTask;
import org.chromium.chromecast.base.Controller;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * Tests for AsyncTaskRunner.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowAsyncTask.class})
public class AsyncTaskRunnerTest {
    private static class TestExecutor implements Executor {
        private final List<Runnable> mTasks = new ArrayList<>();

        @Override
        public void execute(Runnable r) {
            mTasks.add(r);
        }

        public void flush() {
            for (Runnable r : mTasks) {
                r.run();
            }
            mTasks.clear();
        }
    }

    @Test
    public void testSchedulesOnExecutor() {
        List<Integer> result = new ArrayList<>();
        TestExecutor executor = new TestExecutor();
        AsyncTaskRunner runner = new AsyncTaskRunner(executor);
        runner.doAsync(() -> 54, result::add);
        assertThat(result, emptyIterable());
        executor.flush();
        assertThat(result, contains(54));
    }

    @Test
    public void testCloseScopeCancelsTask() {
        List<Integer> result = new ArrayList<>();
        TestExecutor executor = new TestExecutor();
        AsyncTaskRunner runner = new AsyncTaskRunner(executor);
        runner.doAsync(() -> 42, result::add).close();
        executor.flush();
        assertThat(result, emptyIterable());
    }

    @Test
    public void testUseControllerToMakeSureOnlyOneInstanceOfTaskIsRunningAtATime() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        TestExecutor executor = new TestExecutor();
        AsyncTaskRunner runner = new AsyncTaskRunner(executor);
        // For each message in the controller, schedule a task to capitalize the message, and add
        // the capitalized message to the result list.
        controller.subscribe(message -> runner.doAsync(() -> message.toUpperCase(), result::add));
        // If the task is run before the controller is reset, it should add to the list.
        controller.set("new");
        executor.flush();
        assertThat(result, contains("NEW"));
        result.clear();
        // If the controller is reset before the task is run, the result list should be unaffected.
        controller.set("old");
        controller.reset();
        executor.flush();
        assertThat(result, emptyIterable());
        result.clear();
        // If the controller is set with a new value while a task is running, cancel the old task.
        controller.set("first");
        controller.set("second");
        executor.flush();
        assertThat(result, contains("SECOND"));
    }
}
