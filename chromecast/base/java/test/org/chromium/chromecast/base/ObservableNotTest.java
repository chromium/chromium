// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for Observable.not().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableNotTest {
    @Test
    public void testNotIsActivatedAtTheStart() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable.not(invertThis).subscribe(x -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        assertThat(result, contains("enter inverted"));
    }

    @Test
    public void testNotIsDeactivatedAtTheStartIfSourceIsAlreadyActivated() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        invertThis.set("way ahead of you");
        Observable.not(invertThis).subscribe(x -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        assertThat(result, emptyIterable());
    }

    @Test
    public void testNotExitsWhenSourceIsActivated() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable.not(invertThis).subscribe(x -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        invertThis.set("first");
        assertThat(result, contains("enter inverted", "exit inverted"));
    }

    @Test
    public void testNotReentersWhenSourceIsReset() {
        Controller<String> invertThis = new Controller<>();
        List<String> result = new ArrayList<>();
        Observable.not(invertThis).subscribe(x -> {
            result.add("enter inverted");
            return () -> result.add("exit inverted");
        });
        invertThis.set("first");
        invertThis.reset();
        assertThat(result, contains("enter inverted", "exit inverted", "enter inverted"));
    }
}
