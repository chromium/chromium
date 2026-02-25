// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chromecast.base.Controller;

import java.util.ArrayList;
import java.util.List;

/** Tests for AsyncTaskRunner. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AsyncTaskRunnerTest {
    @Test
    public void testSchedulesOnExecutor() {
        List<Integer> result = new ArrayList<>();
        AsyncTaskRunner runner = new AsyncTaskRunner(RobolectricUtil.getPausedExecutor());
        runner.doAsync(() -> 54, result::add);
        assertThat(result, emptyIterable());
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(result, contains(54));
    }

    @Test
    public void testCloseScopeCancelsTask() {
        List<Integer> result = new ArrayList<>();
        AsyncTaskRunner runner = new AsyncTaskRunner(RobolectricUtil.getPausedExecutor());
        runner.doAsync(() -> 42, result::add).close();
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(result, emptyIterable());
    }

    @Test
    public void testUseControllerToMakeSureOnlyOneInstanceOfTaskIsRunningAtATime() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        AsyncTaskRunner runner = new AsyncTaskRunner(RobolectricUtil.getPausedExecutor());
        // For each message in the controller, schedule a task to capitalize the message, and add
        // the capitalized message to the result list.
        controller.subscribe(message -> runner.doAsync(() -> message.toUpperCase(), result::add));
        // If the task is run before the controller is reset, it should add to the list.
        controller.set("new");
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(result, contains("NEW"));
        result.clear();
        // If the controller is reset before the task is run, the result list should be unaffected.
        controller.set("old");
        controller.reset();
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(result, emptyIterable());
        result.clear();
        // If the controller is set with a new value while a task is running, cancel the old task.
        controller.set("first");
        controller.set("second");
        RobolectricUtil.runAllBackgroundAndUi();
        assertThat(result, contains("SECOND"));
    }
}
